### All
You need to be using Node version 12

With [nvm](https://github.com/nvm-sh/nvm#installing-and-updating)

```
nvm install 12
nvm use 12
```


### Windows
In order to build the package on Windows you need to follow the following steps

1. Install [Chocolatey](https://chocolatey.org/install)
2. `choco install -y mingw`
3. Install windows build tools `npm install -g --production windows-build-tools@^3.1.0`
4. Install [Visual Studio 2015 Community Edition](https://my.visualstudio.com/Downloads?q=visual%20studio%202015&wt.mc_id=o~msft~vscom~older-downloads)
5. Install C++ tools on Visual Studio 2015 Community Edition `Add or remove programs -> Visual studio community 2015 -> modify` Then click `Languages -> Visual C++`
