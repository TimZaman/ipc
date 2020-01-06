#include <torch/extension.h>

#include <iostream>
#include <vector>
#include <semaphore.h>
#include <thread> 
#include <unistd.h>  // usleep
#include <sys/socket.h>


static const int parentsocket = 0;
static const int childsocket = 1;

struct packet {
    int n_input_bytes;  // Amount of bytes in the encoded packet
};

class TVL {
  public:

    void worker_thread(int handle) {
        // There are as many worker threads as there are handles.
        std::cout << "worker_thread(handle=" << handle << ")" <<  std::endl;
        const int socket = this->socketpair_fds[handle][parentsocket];

        while (1) {

            packet p;
            int n = read(socket, &p, sizeof(packet));
            std::cout << ::getpid() << " Received packet n_input_bytes=" << p.n_input_bytes << std::endl;

            auto input_buffer = this->input_buffers_[handle];
            // Obtain the pointer to the shared memory input buffer.
            uint8_t * ptr = input_buffer.data_ptr<uint8_t>();

            // Print out the contents of the shared memory input buffer.
            std::cout << "Received contents: ";
            for (int i=0; i<p.n_input_bytes; i++) {
                std::cout << ptr[i];
            }
            std::cout << std::endl;

            // HEAVY CUDA PROCESSING HAPPENS HERE
            usleep(2000000); // Fake processing
            auto output_buffer = this->output_buffers_[handle];
            output_buffer[0] = 13;
            output_buffer[1] = 37;

            // Send back to the child that the processing has completed.

            std::cout << ::getpid() << " Sending packet:" << p.n_input_bytes << std::endl;

            int status = 0;
            write(socket, &status, sizeof(int));
        }

    }

    std::vector<std::array<int, 2>> socketpair_fds;
    std::vector<sem_t *> semaphores_;

    int handles_;
    std::vector<torch::Tensor> input_buffers_;
    std::vector<torch::Tensor> output_buffers_;
    
    
    TVL(const int handles, const  std::vector<torch::Tensor> &input_buffers, const std::vector<torch::Tensor> &output_buffers) {
        std::cout << "ctor pid=" << ::getpid()<< " TVL::TVL(handles=" << handles << ")" << std::endl;

        handles_ = handles;
        input_buffers_ = input_buffers;
        output_buffers_ = output_buffers;

        this->socketpair_fds.resize(handles);
        this->semaphores_.resize(handles);

        for (int i=0; i<handles; i++){
            // Create the socket pairs.
            socketpair(PF_LOCAL, SOCK_STREAM, 0, this->socketpair_fds[i].data());

            // Create the worker threads.
            std::thread t(&TVL::worker_thread, this, i);
            t.detach();  // Who cares about cleanup.

            // Create the named semaphores. These indicate if users have a handle on the system.
            // TODO(tzaman): just the use unnamed semaphores when on UNIX.
            std::string sem_name = "tvl_sem_" + std::to_string(i);
            unsigned int sem_value = 1;
            sem_unlink(sem_name.c_str());  // Total hack.
            sem_t * sem = sem_open(sem_name.c_str(), O_CREAT | O_EXCL, 0644, sem_value);
            if (sem == SEM_FAILED) {
                std::cout << "Error creating semaphore=" << sem << std::endl;
            }

            // Unnamed semaphore implementation (for UNIX)
            // unsigned int sem_value = 1;
            // sem_t * sem;
            // if (sem_init(sem, 1, sem_value) == -1) {
            //     perror("sem_init");
            // }
            
            this->semaphores_[i] = sem;
        }
    }

    ~TVL() {
        std::cout << "TVL::~TVL()" << std::endl;
    }

    int get_handle() {
        std::cout << "TVL::get_handle()" << std::endl;

        while (1) {
            // Try all semaphores until we find a free one
            for (int i=0; i < this->handles_; i++){
                sem_t * sem = this->semaphores_[i];
                if(sem_trywait(sem) < 0) {  // Improve this terrible spinlock
                    // perror("sem_wait");
                    // usleep(10000);
                } else {
                    std::cout << "Got handle on" << i << std::endl;
                    return i;
                }
            }

        }

        return 0;
    }

    void release_handle(const int handle) {
        std::cout << "TVL::release_handle(" << handle << ")" << std::endl;
        sem_t * sem = this->semaphores_[handle];
        if (sem_post(sem) == -1) {
            perror("sem_post");
        }
    }

    torch::Tensor remote_decode(const int handle, const py::bytes &data) {

        std::cout << "remote_decode(handle=" << handle << ")" << std::endl;

        torch::Tensor input_buffer = this->input_buffers_[handle];

        // Convert the bytes data
        std::string s = data;
        int n_input_bytes = s.size();
        for (int i=0; i < n_input_bytes; i++){
            input_buffer[i] = s[i];
        }

        // int64_t * ptr = x.data_ptr<int64_t>();

        const int socket = this->socketpair_fds[handle][childsocket];

        packet p;
        p.n_input_bytes = n_input_bytes;

        std::cout << ::getpid() << " Sending packet:" << p.n_input_bytes << std::endl;
        write(socket, &p, sizeof(packet));

        // Read the status back
        int status;
        int n = read(socket, &status, sizeof(int));

        if (status) {
            std::cout << "Error reported by worker, status=" << status << std::endl;
        }

        torch::Tensor output_buffer = this->output_buffers_[handle];
        return output_buffer;
    }

};


PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
    py::class_<TVL>(m, "TVL")
        .def(py::init<int, std::vector<torch::Tensor>, std::vector<torch::Tensor>>())
        .def("get_handle", &TVL::get_handle)
        .def("release_handle", &TVL::release_handle)
        .def("remote_decode", &TVL::remote_decode, "decode");
}