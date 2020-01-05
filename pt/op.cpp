#include <torch/extension.h>

#include <iostream>
#include <vector>
#include <semaphore.h>
#include <thread> 
#include <unistd.h>  // usleep
#include <sys/socket.h>

const char* SEM_NAME = "my_semaphore";


torch::Tensor decode(py::bytes data, std::vector<int> frame_indices) {
    // This is the actual stand-along(blocking) decode functionality,
    // which should run in the main process as it requires the GPU.
    std::cout << "::decode(" << data << ")" << std::endl;

    // Decoding happens here
    // Note we can infer the size of all the frames from the decoded sequence.
    // TODO(tzaman): How do you allocate torch shared memory with their C++ API?
    auto frames = torch::zeros({1, 640, 480, 6}); // [nframes, width, height, channels]
    return {
        frames
    };
}







class MyClass {
  public:

    void worker_thread() {
        std::cout << "worker_thread())" <<  std::endl;

        printf("calling sem_wait\n");
        if(sem_wait(this->sem_) < 0) {
            printf("sem_wait error\n");
        }
        printf("sem was set!!!\n");
        // Perform processing here?
        usleep(3000000);
        sem_post(this->sem_);
    }

    MyClass(int number) {
        std::cout << "MyClass::MyClass(" << number << ")" << std::endl;

        /* initialize semaphores for shared processes */
        this->ctor_pid_ = ::getpid();
        std::cout << "ctor pid=" << ::getpid() <<std::endl;
        unsigned int sem_value = 1; 

        // Create a named semaphore.
        sem_unlink(SEM_NAME);  // Total hack.
        this->sem_ = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0644, sem_value);
        if (this->sem_ == SEM_FAILED) {
            std::cout << "Error creating semaphore=" << this->sem_ << std::endl;
        }

        // Create an unnamed semaphore (not supported on MacOS!)
        // int ret = sem_init(this->sem_, 0, sem_value);
        // if (ret) {
        //     std::cout << "sem_init error ret=" << ret << std::endl;
        // }



        std::thread t(&MyClass::worker_thread, this);
        t.detach();  // I don't give a shit about cleanup.
        



    }
    ~MyClass() {
        // TODO(tzaman): cleaning up could be tricky, as we likely lost track of who's using
        // the semaphore XD.
        std::cout << "MyClass::~MyClass()" << std::endl;
        // if (::getpid() == this->ctor_pid_){
        //     // The creator process should clean up any semaphores.
        //     sem_unlink(SEM_NAME);   
        //     sem_close(this->sem_);  
        // }
    }
    int ctor_pid_;
    sem_t* sem_ = nullptr;
    std::string debug(){
        std::cout << "MyClass::debug()"  <<std::endl;
        std::cout << "@debug pid=" << ::getpid() <<std::endl;
        int val = -1;
        // int ret = sem_getvalue(this->sem_, &val);
        // if (ret) {
        //     std::cout << "@debug ret=" << ret << std::endl;
        // }
        std::cout << "@debug sem_=" << this->sem_ << " val=" << val << std::endl;

        // if (sem_post(this->sem_) == -1) {
        //     printf("sem_post() failed\n");
        // }
        

        return "Foobar";
    }

    // torch::Tensor remote_decode(py::bytes data, std::vector<int> frame_indices) {
    torch::Tensor remote_decode(py::bytes data) {
        // This should be used if the caller is not the main process and hence has no CUDA
        // context. It will communicate with the main process (with the CUDA context) through ipc.
        std::cout << "remote_decode()" << std::endl;
        worker_thread();
        auto frames = torch::zeros({0});
        return {
            frames
        };
    }

};


PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
    m.def("decode", &decode, "decode");
    py::class_<MyClass>(m, "MyClass")
        .def(py::init<int>())
        .def("debug", &MyClass::debug)
        .def("remote_decode", &MyClass::remote_decode, "decode");
}