# Simulations for TrustNet


Compile NS3 with BRITE integration and PyViz.
(Linux/x86 preferred)

Put this repo inside the `scratch` directory.
Then from the parent ns3 directory, run

```bash
./ns3 run scratch/trustnet/main.cc --vis
```

If you don't want visualization, remove `--vis`.