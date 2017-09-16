# teamspeak-plugin-ducker

### Ducking
Global Ducking: Reduce the volume of musicbots when someone talks.

Channel Ducking: Reduce the volume of home or foreing tabs when someone talks on the other one or you receive a whisper.

![Channel Ducking](https://github.com/thorwe/CrossTalk/raw/master/misc/ct_screenie_duck.png "Channel Ducking")

## Installation

It is recommended to install the plugin directly from within the client, so that it gets automatically updated.
In you TeamSpeak client, go to Tools->Options->Addons->Browse, search for the "Ducker" plugin and install.

## Compiling yourself
After cloning, you'll have to manually initialize the submodules:
```
git submodule update --init --recursive
```

Qt in the minor version of the client is required, e.g.

```
mkdir build32 & pushd build32
cmake -G "Visual Studio 15 2017" -DCMAKE_PREFIX_PATH="path_to/Qt/5.6/msvc2015" ..
popd
mkdir build64 & pushd build64
cmake -G "Visual Studio 15 2017 Win64"  -DCMAKE_PREFIX_PATH="path_to/Qt/5.6/msvc2015_64" ..
```
