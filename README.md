# fivedxp4mt5
this branch is for mt5 exp (SVNR 12357)

# additional library file
NVIDIA Performance Analysis Kit (libNVPerfSDK.so.1)

> x32
> http://developer.download.nvidia.com/tools/NVPerfKit/6.0/NVPerfKit-Linux-x86-173.13.tar.gz

> x64
> http://developer.download.nvidia.com/tools/NVPerfKit/6.0/NVPerfKit-Linux-x86_64-173.13.tar.gz

# Common Sense is REQUIRED
### if your still using config.json, you will need to change it to config.toml

# setup
- copy everything in Dist folder and libfivedxp.so to the game folder
- make a folder named ```libso``` and put libcrypto.so.0.9.8 & libprotobuf.so.7 & libssl.so.0.9.8 in it
- do ```chmod +x start.sh```
- start the game
- enable test menu
- open game options
- disable steering power
- open i/o test and go to i/o interface initialize

# starting the game
- ```./start.sh```

# credits
https://github.com/jmpews/Dobby
https://github.com/OpenJVS/OpenJVS