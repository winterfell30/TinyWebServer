# TinyWebServer

一个Linux环境下基于线程池的Web服务器，支持GET、POST、HEAD请求，可以提供静态服务和动态服务。  
静态服务是通过磁盘取得文件并返回给客户端，支持jpg、gif、html和无格式文本文件。  
动态服务是在服务器上一个子进程的上下文中运行一个程序并将结果返回给客户端，例程中附带了一个加法的方法  
生产者接收连接将连接描述符放入buffer中，消费者从共享buffer中取描述符提供服务    

使用方法：  
1.  
make  
2.  
./main port  
3.  
可以使用telnet模拟访问，也可以直接在web浏览器url栏输入IP:port访问。  
通过修改URL可以访问服务器文件及含参使用动态服务，也可以在HTML表单中直接请求（例程request.html)   
可以自行添加动态服务方法，将可执行文件放在cgi-bin文件夹下即可调用    
