# Instructions

## constitution
创建原则聚焦于：ARM架构内存优化、实时包处理性能（延迟<10ms）、车规级可靠性（ISO 26262意识）、Suricata规则兼容性、Android NDK集成标准、嵌入式功耗效率、安全优先编码实践，以及覆盖单元测试、集成测试、模糊测试、性能测试和硬件在环验证的完整测试体系。

## specify

基于Suricata开发网络入侵检测及深度包检测功能，支持跑在车载ARMv8架构、操作系统Android12+平台。 

基本想法
引入的Suricata需要开源合规，采用规避GPL传染的架构设计，通过进程隔离，让私有代码与GPL组件保持"井水不犯河水"的边界，避免GPL传染。

内置基础规则，支持规则的热更新。

以太网入侵检测 
解析2到7层数据包，匹配入侵检测策略，生成入侵检测安全日志

DPI深度包检测
支持对SomeIP、DoIP、Http、TLS、DNS等应用层协议的深度包解析，按照协议字段配置的规则，对报文进行检测，产生安全事件日志。

支持TCP land攻击、TCP ACK flood 攻击、TCP畸形报文攻击、UDP flood攻击、UDP畸形报文攻击、TCP SYN flood攻击、ICMP flood攻击、Large ping攻击、DoIP协议监控、MQTT协议监控、GB/T 32960.3协议监控、SOME/IP协议监控、TCP数据包异常监控、UDP数据包异常监控、IGMP flood攻击、Telnet弱密码监控、Ftp弱密码监控、杂项监控、漏洞检测等安全事件的监控，产生安全事件日志。


## plan

将 Andorid Application改成native层的bin程序


## test

优化Makefile，只保留Android相关内容；基于Docker + 模拟器搭建测试环境，完成单元测试和集成测试相关功能；补充测试数据和对应测试用例


请执行所有测试用例，如果报错，think ultra hard，并修复对应缺陷，确保所有测试用例正常通过



