import os
import time

import torch
import torch.utils.cpp_extension

print()


class TVLHandle:
    def __init__(self, tvl):
        print('TVLHandle')
        self._tvl = tvl
        self.handle = None

    def __enter__(self):
        print('__enter__')
        self.handle = self._tvl.backend.get_handle()
        return self

    def __exit__(self, type, value, traceback):
        print('__exit__')
        self._tvl.backend.release_handle(self.handle)
        self.handle = None

    @property
    def input_buffer(self):
        return self._tvl.input_buffers[self.handle]

    @property
    def output_buffer(self):
        return self._tvl.output_buffers[self.handle]

    def remote_decode(self, data):
        print(f'remote_decode with self.handle={self.handle}')
        return self._tvl.backend.remote_decode(self.handle, data)


class TVL:
    op = torch.utils.cpp_extension.load(name="op_cpp", sources=["op.cpp"])

    def __init__(self, handles):
        # To avoid complexity, we use torch's to manage our (shared) memory. For transparency,
        # this is defined here in python so we can easily debug things. We pass it to the C++
        # operation later so that it can access its contents.
        self.input_buffers = []
        self.output_buffers = []
        for i in range(handles):
            # Create one input and output shared memory buffer per handle
            self.input_buffers.append(torch.zeros([1000000], dtype=torch.uint8).share_memory_())
            self.output_buffers.append(torch.zeros([4, 2], dtype=torch.uint8).share_memory_())

        # Share the shared memory buffers with the C++ op so that it can access it.
        self.backend = self.op.TVL(handles, self.input_buffers, self.output_buffers)

    def get_handle(self):
        return TVLHandle(tvl=self)


tvl = TVL(handles=2)



class MyDataset(torch.utils.data.Dataset):
    def __init__(self):
        super(MyDataset).__init__()

    def __len__(self):
        return 32

    def __getitem__(self, i):
        with tvl.get_handle() as h:
            encoded_data = bytes(f'__getitem__({i})'.encode('utf8'))
            frames = h.remote_decode(encoded_data)
            # print(f'frames={frames}')
        
        # time.sleep(1)

        return i


dataset = MyDataset()
loader = torch.utils.data.DataLoader(dataset, num_workers=2, batch_size=2)


for i in loader:
    print(i)