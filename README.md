# BMC Apps

This is repository of useful demons for BMC. This application are proof of concept for the [Reactor](https://github.com/abhilashraju/reactor) library. 

The applications themselves are useful for various usecases we see in BMC.

Following are the list of applications.
- Agrregator: A redfish aggregator that can scatter the redfish request from clients to multiple satelite BMCs. Aggregate the responses according to agrregator schema. 
- FileSync: A Filesync service that can sync data between BMC using a mutually trusted encryption channel. 
- CredCopier: A Tcpclient that can make connection to remote FileSync server to transfer confidential data through a mutually trusted encrypted channel.
- Reverse Proxy: A reverse proxy implementation that act as an entry point for all redfish endpoint. Using a configuration file BMC can decide to whome the purticular request should be forwarded to. 
- HttpServer: A Http Server implimentation that shows how easy it is to write a webserver using the reactor framework.

The backbone of the application is the [Reactor](https://github.com/abhilashraju/reactor) library, which lifts some useful client/server abstractions out of boost::asio and boost::beast libraries. 


### Architeture

![](./images/reactor.svg)