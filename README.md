# Simulations for TrustNet
TrustNet exposes a new Internet-scale architecture for routing-
based trust policies of end users. It allows packets to flow
only through domains that the user trusts to not leak meta-
data.

## Table of Contents
* [Prerequisites](#prerequisites)
* [Run the simulation](#run-the-simulation)
* [Collecting performance measures](#collecting-performance-measures)
* [Parameter change](#parameter-change)
    * [Modify number of Users](#modify-number-of-users)
    * [Modify Users' packet sending rate](#modify-users-packet-sending-rate)
    


## Prerequisites

* NS3 (version 3.38 preferred, 3.37 works)
* BRITE integration with NS3 (check [tutorial](https://www.nsnam.org/docs/models/html/brite.html))
* (Optional visualization) PyViz Integration with NS3 (???)


## Run the simulation

First of all, our simulation covers multiple cases (see our paper's Evaluation section). 

Commit Hash correspondences:
* **Border**: main / 41bce0d9ed5c8b627f309e2cc223ddbb7db5d83f
* **Direct**: c84060d0234bf81cb262f606bd2b59eb0ff4ae0c
* **RR**: 61b3458663425dcea7b17db350f3e5398e9c3c42
* **Closest**: 93cde5876b1a31b0eeb00d1646dc97e29ce07034

Take main branch as an example, to run the simulation for the **Border** case:
1. Create a `traces/` directory within the NS3 directory
2. Clone this repo and name it `trustnet` inside the `scratch` directory of NS3.
3. Then from the parent NS3 directory, run

```bash
./ns3 run scratch/trustnet/main.cc 2>debug.log
```


If you need visualization, append `--vis` at the end.

## Collecting performance measures
At the same directory as the above command, run
```bash
grep "Receive delay:" debug.log | awk '{ total1 +=$3; count++; total2 += $5} END {print total1/count " " total2/count " " count}'
```
The result is a tuple of three numbers: `(Average Latency | Average Hop Count | # Messages Passed)`

## Parameter change

Note that if any specific code line number is referenced below to change a certain parameter, they **only** make sense for the main branch as we take the **Border** simulation case as an example to illustrate how the parameters can be changed. Nevertheless, similar changes around the same location in the code can be made in other aforementioned branches correspondingly.

### Modify number of Users
Go to `scratch/trustnet/main.cc : line 526`, you can see there are in total 8 calls to `BUILD_CLIENT(...)` macro. By default, we are enabling 2 Users. You can uncomment/comment these lines to adjust the number of Users presented in the simulation

### Modify Users' packet sending rate
Go to `scratch/trustnet/dummy_client2.cc : line 568`, you can adjust the numeric values of the scheduling delay of the sending behavior of all Users.
