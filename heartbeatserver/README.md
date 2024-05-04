### Hearbeat Server

A heart beat monitor based on UDP Server from the  [Reactor](https://github.com/abhilashraju/reactor) library. The server monitors periodic pings from peer devices. The pings are UDP packets. The server runs in dual mode 
- As a Heartbeat Server
- As a Heartbeat Client

Which mean the server sends its own heart beats to peer servers. 
This way we can create a mutually observing peers in a distirbuted enviroment.

The server can be started with a configuration file as below
```
{
    "port": "80833",
    "interval": 1,
    "targets": [
        [
            "169.254.62.244",
            "80833"
        ]
    ]
}
```

- port: The port number where the UDP server listen to
- interval: The ping interval
- targets: Set of peer endpoints, that should be monitored by this server