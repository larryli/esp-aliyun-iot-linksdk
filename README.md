# esp-aliyun-iot-linksdk
Aliyun IOT LinkSDK 4.x for esp-idf

## 使用

### 新项目

    idf.py create-project foobar
    cd foobar
    mkdir components
    git init
    git submodule add https://github.com/larryli/esp-aliyun-iot-linksdk components/aiot_sdk
    idf.py build

### 拉取旧项目

    git clone foobar
    cd foobar
    git submodule sync --recursive
    git submodule update --init --recursive

### 在项目中更新组件

    cd foobar
    git pull --recurse-submodules

## SDK 功能

[SDK 定制](https://iot.console.aliyun.com/lk/document/tools)页面选择如下：

- SDK 版本：v4.x
- 设备 OS：FreeRTOS
- 设备硬件形态：单板系统
- 连接物联网平台协议：
  - [x] MQTT 3.1.1
  - [ ] HTTPS
- 数据加密：无加密
- 设备认证方案：设备密钥
  - [ ] 动态注册
- 高级能力
  - [x] 物模型
  - [x] OTA
  - [x] 时间同步
  - [x] 设备影子
  - [x] 设备日志
  - [x] 设备标签
  - [x] 引导服务
  - [x] 子设备管理
  - [x] 设备诊断
  - [x] 任务管理
- 资源消耗预估

功能配置 | 说明 | RAM 预估 | ROM 预估
----|----|----|----
基础函数库 | 为连云等功能提供基础函数 | 0.421 KB | 9.843 KB
MQTT | 基于MQTT协议连接阿里云物联网平台 | 0.885 KB | 9.476 KB
物模型 | 使用属性、事件、服务构成的物模型描述设备 | 0.151 KB | 4.043 KB
OTA | 固件升级和远程配置 | 0.767 KB | 7.012 KB
时间同步 | 基于MQTT协议从云平台获取标准时间 | 0.15 KB | 1.48 KB
设备影子 | 基于MQTT协议将设备的状态上报和云端的指令下发缓存在云端JSON文档中 | 0.156 KB | 2.049 KB
设备日志 | 基于MQTT协议将设备日志上报到云端 | 0.184 KB | 1.749 KB
设备标签 | 基于MQTT协议向云平台操作设备标记 | 0.215 KB | 1.629 KB
引导服务 | 引导全球设备就近连接服务器 | 1.065 KB | 3.14 KB
子设备管理 | 基于MQTT协议的子设备注册，拓扑管理和上下线功能 | 1.532 KB | 19.166 KB
设备诊断 | 基于MQTT协议,将设备诊断信息上报到云端 | 0.264 KB | 5.973 KB
任务管理 | 设备端任务的下发与管理 | 0.156 KB | 3.728 KB
总计 | | **5.946 KB** | **69.288 KB**

**注：** HTTPS、TLS 和动态注册均会引入外部 mbedtls 代码，所以不提供相关功能。 
