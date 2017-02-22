# TinyWebServer

一个简易的Linux环境下的Web服务器，支持GET请求，可以提供静态服务和动态服务。  
静态服务是通过磁盘取得文件并返回给客户端，支持jpg、gif、html和无格式文本文件。  
动态服务是在服务器上一个子进程的上下文中运行一个程序并将结果返回给客户端，例程中附带了一个加法器。  

使用方法：  
1.  
```
make
```  
2.  
```  
./main port
```
3.  
(1).  
在终端中使用telnet访问服务器  
```
telnet localhost port
```  
(2).   
直接在浏览器中使用localhost:port访问服务器  