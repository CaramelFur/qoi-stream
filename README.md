# qoi-stream

Simple C based one byte at a time streaming qoi decoder

There were already some qoi encoders that could "stream", but they still required a reference to the entire dataset, and tried to decode multiple bytes at a time. I do not see that as true streaming.
So I decided to write one that did actually decode the qoi image one byte at a time. This can be useful for many things, but its mainly useful when you have little memory.
