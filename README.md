# Simulations for TrustNet
TrustNet exposes a new Internet-scale architecture for routing-
based trust policies of end users. It allows packets to flow
only through domains that the user trusts to not leak meta-
data.

[link?]()

## Table of Contents
* Prerequisites
* Run the simulation
* Collecting performance measures
* Parameter change
    * Modify number of Users
    * Modify Users' packet sending rate
    


## Prerequisites

* NS3 (version 3.38 preferred, 3.37 works)
* BRITE integration with NS3 (check [tutorial](https://www.nsnam.org/docs/models/html/brite.html))
* (Optional visualization) PyViz Integration with NS3 (???)


## Run the simulation

First of all, our simulation covers multiple cases (see paper eval section link???). Each simulation case is in its separate branch. 

Branch correspondance:
* **Border**: main
* **Direct**: direct
* **RR**: RR
* **Closest**: closest

Take main branch as an example, to run the simulation:
clone this repo and name it `trustnet` inside the `scratch` directory of NS3.
Then from the parent NS3 directory, run

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