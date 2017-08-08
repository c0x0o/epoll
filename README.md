# EPOLL example

## intro

这是一个epoll回射服务器和客户端的示例程序。客户端同时也可以产生高并发连接对服务器进行压力测试。

This is an epoll-based echo programs(including echo server and echo client). The client can generate concurrent requests to test the performance of echo server.

使用`server -h`或者`tester -h`来获取更多帮助。

use `server -h` or `tester -h` to get more help.

## Tips

项目中使用了bipbuffer作为缓冲区模块，详情请参见[DanteLee/bipbuffer](https://github.com/DanteLee/bipbuffer)。

We use bipbuffer to manage our buffer module, see [DanteLee/bipbuffer](https://github.com/DanteLee/bipbuffer) for more details.

