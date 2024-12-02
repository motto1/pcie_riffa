# PCIE DLL 使用说明文档

## 简介
这是一个用于PCIE设备通信的动态链接库，基于RIFFA框架开发，提供了与FPGA设备进行数据交互的接口。主要功能包括设备检测、打开/关闭设备、数据收发、FIFO操作以及版本信息查询等。

## 主要功能
1. PCIE设备基础操作（检测、打开、关闭）
2. 数据发送和接收
3. 500MB大小的FIFO缓冲区管理
4. 版本信息查询
5. 详细的日志记录系统

## 详细API说明

### 1. 基础设备操作

#### `int checkPcie()`
- 功能：检查系统中可用的PCIE设备
- 返回值：找到的设备数量
- 使用示例：
```cpp
Pice_dll* pcie = new Pice_dll();
int devices = pcie->checkPcie();
if (devices > 0) {
    qDebug() << "找到" << devices << "个PCIE设备";
}
```

##### 基础设备操作
1. `int checkPcie()`
   - 功能：检查系统中可用的PCIE设备
   - 返回值：找到的设备数量
   - 使用示例：
   ```cpp
   Pice_dll* pcie = new Pice_dll();
   int devices = pcie->checkPcie();
   ```

2. `int openPcie()`
   - 功能：打开PCIE设备
   - 返回值：0表示成功，其他值表示失败
   - 使用示例：
   ```cpp
   if(pcie->openPcie() == 0) {
       // 设备打开成功
   }
   ```

3. `void closePcie()`
   - 功能：关闭PCIE设备并清理资源
   - 使用示例：
   ```cpp
   pcie->closePcie();
   ```

##### 版本信息 //TODO
1. `VersionInfo getVersionInfo()`
   - 功能：获取设备的硬件和软件版本信息
   - 返回值：包含硬件版本和软件版本的结构体
   - 使用示例：
   ```cpp
   Pice_dll::VersionInfo version = pcie->getVersionInfo();
   QString hwVer = version.hardwareVersion;
   QString swVer = version.softwareVersion;
   ```

##### FIFO操作
1. `bool fpga_fifo(int value)`
   - 功能：启动FIFO操作，创建写入线程
   - 参数：value - 数据包大小
   - 返回值：true表示成功，false表示失败
   - 使用示例：
   ```cpp
   if(pcie->fpga_fifo(10)) {
       // FIFO操作启动成功
   }
   ```
   - 特点：
     - 自动管理500MB循环缓冲区
     - 自动处理缓冲区回环
     - 包含组计数器限制（5000组）

2. `bool fpga_read(unsigned int* read_buffer, int timeout_ms = 1000)`
   - 功能：从FIFO读取数据
   - 参数：
     - read_buffer: 读取数据的缓冲区
     - timeout_ms: 超时时间（默认1秒）
   - 返回值：true表示成功，false表示失败
   - 使用示例：
   ```cpp
   unsigned int buffer[1024];
   if(pcie->fpga_read(buffer, 1000)) {
       // 数据读取成功
   }
   ```
   - 特点：
     - 自动处理缓冲区回环
     - 支持超时机制
     - 保证数据完整性

##### 版本信息 //TODO
1. `VersionInfo getVersionInfo()`
   - 功能：获取设备的硬件和软件版本信息
   - 返回值：包含硬件版本和软件版本的结构体
   - 版本格式：v主版本.次版本.修订号

##### 日志控制
1. `static void enableRiffaLog(bool enable)`
   - 功能：控制RIFFA底层日志输出
   - 默认：关闭

2. `static void enablePiceDllLog(bool enable)`
   - 功能：控制PICE DLL日志输出
   - 默认：开启

#### 状态查询
1. `QString getLastError() const`
   - 功能：获取最后一次错误信息

2. `bool isConnected() const`
   - 功能：检查设备是否已连接
   - 返回值：true表示已连接，false表示未连接

3. `size_t getCurrentFifoPos() const`
   - 功能：获取当前FIFO写入位置
   - 返回值：当前位置（字节）

4. `size_t getNextReadPos() const`
   - 功能：获取下一个读取位置
   - 返回值：下一个读取位置（字节）

## 重要参数说明
1. FIFO缓冲区：
   - 大小：500MB
   - 类型：循环缓冲区
   - 管理：自动处理回环

2. 组计数限制：
   - 最大组数：5000
   - 计数器：自动管理
   - 超限处理：自动停止

3. 超时设置：
   - 读取超时：默认1000ms
   - 可配置范围：0-∞ ms

## 错误处理
1. 所有可能失败的操作都返回错误状态
2. 使用 getLastError() 获取详细错误信息
3. 日志系统记录关键操作和错误

## 性能优化建议
1. 合理设置包大小
2. 注意监控缓冲区使用情况
3. 及时处理读取的数据
4. 适当配置超时时间

## 线程安全性
1. FIFO操作使用独立线程
2. 读写操作互相独立
3. 自动处理资源竞争

## 使用注意事项
1. 初始化顺序：
   - 先检查设备 (checkPcie)
   - 再打开设备 (openPcie)
   - 最后进行数据操作

2. 资源管理：
   - 使用完毕后调用 closePcie
   - 注意释放缓冲区
   - 处理好线程终止

3. 错误处理：
   - 检查所有返回值
   - 实现错误恢复机制
   - 记录关键错误日志

4. 性能监控：
   - 监控包计数器
   - 关注缓冲区使用率
   - 处理超时情况

## 版本历史
- v1.0.0: 初始版本
  - 基本PCIE通信功能
  - FIFO操作支持
  - 版本信息查询
  - 日志系统
  - 500MB循环缓冲区
