### Reverse Proxy

The Reverse Proxy implemented using the [Reactor](https://github.com/abhilashraju/reactor) framework. 

#### Problem 
 At present all of the redfish end points are implemented in bmcweb. This is fine for all generic use cases. But there are cases where due to buisiness model of the Organization, the implementation of certian redfish end points are not in the interest of community. 
Bmcweb does not have good extension points to support such redfish APIs. 

#### Solution

Reverse proxy can be used as single entry point to the BMC system  for any redfish clients. The proxy can be configured to forward the request to corresponding backend service that can cater the redfish API. While most of the request land in Bmcweb , all business specfic APIs can be forwarded to corresponding services that implements it.

Example of proxy.conf

```
[ 
    {
        "path": "/redfish/v1/*",
        "ip": "localhost",
        "port": "https",
        "default": true
    },
    {
        "path": "/redfish/v1/Systems/system/LogServices/Dump/*",
        "ip": "localhost",
        "port": "8082"
    },
	{
        "path": "/redfish/v1/LicenseService/*",
        "ip": "localhost",
        "port": "8083"
    }
]
```

Note that it is not mandatory for the services to implement https. Since the communication between proxy and services are internal, services can implement non encrypted end points too. 