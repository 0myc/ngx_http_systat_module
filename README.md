# ngx_http_systat_module
System status module for nginx

### Features

* Linux/FreeBSD/MacOS (on Linux/MacOS only 32-bits counters)

### Build

cd to NGINX source directory & run this:

    ./configure --add-module=<path-to-ngx_http_systat_module>
    make
    make install


### Example nginx.conf

    http {
        server {
            listen       80;
            server_name  _;

            location /tx_bytes {
                systat netif_tx_bytes eth0;
            }
        }
    }

### Directives
#### systat
Syntax: `systat netif_tx_bytes` _net_interface_   
Context: location

Sets statistics handler to the current location.


### TODO

* new metrics: netif_rx_bytes, netif_tx_packets, netif_rx_packets
* new metrics: load avg, CPU usage
* output formats: plain, xml
