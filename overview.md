## general operation
+ send location and events report --- GPS和事件上报
+ communicate with backend server with IP message or SMS text message---通过IP或SMS和后台服务器进行通信
+ device expect ack for every message transmitted---设备对所有发送的消息进行应答，应答失败进行重传。
+ device will store report/events in absence of cell coverage---在没有网络覆盖的情况下存储报告或事件，在恢复网络的时候再上传。
+ device will accept usr provided configurable parameter values(in range specified)---设备接收用户提供的配置参数，不包含基于文本的参数配置
+ device provide preset values for user configurable parameter---为用户提供预设值。
+ device will ack every remote command received from user--- 对用户的远程命令进行应答
+ device will respond to request for location with a location report---对位置信息的请求上报位置信息
+ device will allow server to update/change configuration---允许服务器更新或改变配置
+ device will send daily heartbeat location report --- **frequency based on configuration**.---上报日常心跳位置报告
+ device will send parking alert event when vehicle has been stationary for more
than one hour---上报停车告警事件
+ device will support circular and polygan geofences.---支持圆形或多边形围栏
  + 服务器管理员可以增加、修改、删除任何围栏
  + 设备每分钟进行围栏事件检测, 设备在检测到任意方向发生穿越围栏时，上报告警事件。

+ 汽车熄火后和正常时表现一致。

## OTA
+ will work with ignition both on and off---无论点火熄火，均正常工作
+ replace the current image with new image---更新固件
+ able to store two main image simultaneously---同时保存新老版本映像
+ error happend in the download process, able to go back to the older one.---更新失败，回退到老版本。
+ will ultilize the microprocessor resources, other capabilities will be suspended
during the process---期间挂起所有其他进程
+ will ultilize flash memory and will overwrite any stored messages if image require more memory
than available---flash空间不够用时，将会覆盖其他区域。
+ upon conclusion or early termination of download, device will report the resulting status---设备上报升级结果。

## gps
+ device will receive gps signal at least once every 15sec when ignition is on---点火后每15秒接收gps信号用于内部使用。
+ device will provide basic sanity check on new GPS location---提供新GPS位置检测
+ if gps location is suspect:---GPS信息可疑时提供：
  * save last good location---保存上一个合理的位置信息
  * provide location aging---提供位置信息老化
+ drift control: define its location based on last "dynamic" GPS fix record---设置停车位置为上一次的动态GPS修正位置

## watch mode
+ device provide initial vehicle location within 30sec---30秒内上报初始化位置信息
+ device provide updates location every 60min---点火后每60分钟（可预置）更新位置信息，直到熄火。

## repo mode/theft mode
+ provide initial vehicle location within 20sec---20秒内上报位置信息
+ updates on location every 3~5min---点火后，每3~5分钟更新位置信息，直到熄火。

- 以上模式中，设备在恢复网络覆盖后主动上报应答。

## late payment/audible warning
+ device trigger notification---点火后通过设置外部蜂鸣器来触发通知。
+ device will notify the user that the late payment was delivered---通知用户告警已传达。

## device engine starter disable/enable vehicle
+ the device will send respond to a request to disable vehicle by sending an ack
to the requet and sending GPS position at the time requeest received---通过应答来disable engine，在收到请求的同时发送gps位置信息
+ condition for disabling the starter:
  + 熄火
  + 汽车静止
  + 有效的网络覆盖
  + 有效的GPS修正
+ **以上条件需同时满足**

## manual starter override
* have following features:---通过预置的点火周期
 + **1** MSO feature will be enabled whenever the vehicle is in starter-disable state;---熄火状态下使能MSO特性
 + **2** there will be a manual override counter to track and determine the number of
 times this feature get used;---计数器跟踪MSO使用次数，减到0时，disable MSO
 + **3** in order to initiate an ignition on/off cycle, the ignition needs to be off
 for 15sec---熄火15秒，用以初始化点火熄火周期。
 + **4** manual override require the following key sequence to re-enable the starter;---重新使能starter时的关键序列。
 + **5** manual counter can be reset;---计数器可以通过配置参数配置
 + **6** "emergency enable"command will override MSO feature.

## device theft/tow away detection and reporting
+ detected via accelerometer
+ enable when ignition is off
+ tow event is generated if there is a change in postion that is more than a predefined
limit during ignition off

## external sensor support
+ sensor connected to analog inputs will be monitored periodically.
+ a user will be able to remotely control digital outputs

## data flow mirroring
+ ablity to mirror the data flow to a second back-up server.

## accelerometer --- “six degrees of motion detection”
+ be able to detect motion during ignition on/off state
+ be able to provide raw accelerometer samples at regular intervals.
* able to wake up device from sleep mode to full power mode"need to be able to tune this feature sensitivity"
* Ability to adjust/tune accelerometer sensitivity so that driver behavior parameters can be set to capture “hard breaking, acceleration, hard turns, impacts, etc
* Always store last 30 seconds of accelerometer data so historical data can be retrieved by system

## device installation
+ device have one LED to indicate cell signal and one LED to indicat GPS signal
+ blink while seraching, solid when signal confirmed.
* Ability to toggle LED’s on or off to save power but not affect device state or preperformance
