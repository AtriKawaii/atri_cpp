# AtriCPP
为[AtriBot](https://github.com/LaoLittle/atri_bot)编写CPP插件

## 开发示例 (CMake)
示例项目结构: 
```text
my_plugin/
├── includes/
│   │
│   atri_plugin.h
├── CMakeList.h
└── plugin.cpp
```

CMakeList.txt: 
```cmake
cmake_minimum_required(VERSION 3.24)
project(my_plugin)

set(CMAKE_CXX_STANDARD 20) # 由于使用u8str, 最低需要C++20

add_library(my_plugin SHARED plugin.cpp)
```

编写插件 (plugin.cpp): 
```c++
#include "includes/atri_plugin.h"

class MyPlugin: public Atri::Plugin {
public:
    ~MyPlugin() override {
        delete guard; // 关闭监听器
        logger::info(u8"插件释放");
    }

public:
    const char8_t * name() override {
        return u8"MyPlugin";
    }

    void enable() override {
        guard = event::Listener::listening_on<event::GroupMessageEvent>([](event::GroupMessageEvent e) {
            int64_t id = e.group().id();

            printf("id=%lld\n", id);
            fflush(stdout);

            return true;
        });
        logger::info(u8"插件已启动");
    }

    void disable() override {
        logger::info(u8"插件禁用");
    }

private:
    event::ListenerGuard *guard = nullptr;
};

ATRI_PLUGIN(MyPlugin) // 此宏必要, 用于导出插件
```

生成构建文件 (以ninja为例, 构建目录为./build): 
```commandline
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -G Ninja
cd build
ninja
```

最后在构建文件夹得到的动态库文件即为插件本体, 将其放入`atri_bot/plugins`内即可

## [本项目并不完善, 建议使用Rust编写插件](https://github.com/AtriKawaii/atri_rust)
