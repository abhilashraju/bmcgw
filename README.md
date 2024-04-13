# BMC Apps

This is repository of useful demons for BMC. This application are proof of concept for the [Reactor](https://github.com/abhilashraju/reactor) library. 

The applications themselves are useful for various usecases we see in BMC.

Following are the list of applications.
- Agrregator: A redfish aggregator that can scatter the redfish request from clients to multiple satelite BMCs. Aggregate the responses according to agrregator schema. 
- FileSync: A Filesync service that can sync data between BMC using a mutually trusted encryption channel. 
- CredCopier: A Tcpclient that can make connection to remote FileSync server to transfer confidential data through a mutually trusted encrypted channel.

The backbone of the application is the [Reactor](https://github.com/abhilashraju/reactor) library, which lifts some useful client/server abstractions out of boost::asio and boost::beast libraries. 


### Architeture

![](./images/reactor.svg)