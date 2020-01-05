import os
import time

import torch
import torch.utils.cpp_extension

print()

op = torch.utils.cpp_extension.load(name="op_cpp", sources=["op.cpp"])


# frames = op.decode(b'blabla', [1])
# print(f'frames={frames}')


# This should instantiate the TVL, which starts C++ worker threads.
a = op.MyClass(1)

# time.sleep(5)

x = a.debug()
# print(f'x={x}')
# exit()

class MyDataset(torch.utils.data.Dataset):
    def __init__(self):
        super(MyDataset).__init__()

    def __len__(self):
        return 10

    def __getitem__(self, i):
        # time.sleep(1)
        a.remote_decode(b'1337', [i])

        # Create an empty buffer to put the files into (preferably pinned and shared)
        # buffer = torch.tensor([32]).share_memory_()

        a.remote_decode(buffer=buffer)

        return i


def worker_init_fn(i):
    print(f'worker_init_fn({i}) pid={os.getpid()}')
    # worker_info = torch.utils.data.get_worker_info()
    # print(f'a={a} a.debug()={a.debug()}')

dataset = MyDataset()
loader = torch.utils.data.DataLoader(dataset, num_workers=1, batch_size=2, worker_init_fn=worker_init_fn)


for i in loader:
    print(i)