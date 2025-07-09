import mqtt from'../../utils/mqtt.js';
const aliyunOpt = require('../../utils/aliyun/aliyun_connect.js');

let that = null;

Page({
  onCall() {
    wx.makePhoneCall({
      phoneNumber: '110',
    });
  },
    data:{

      //设置温度值和湿度值 
      longitude:0,
      latitude:0,
      LightSwitch:0,

      client:null,//记录重连的次数
      reconnectCounts:0,//MQTT连接的配置
      options:{
        protocolVersion: 4, //MQTT连接协议版本
        clean: false,
        reconnectPeriod: 1000, //1000毫秒，两次重新连接之间的间隔
        connectTimeout: 30 * 1000, //1000毫秒，两次重新连接之间的间隔
        resubscribe: true, //如果连接断开并重新连接，则会再次自动订阅已订阅的主题（默认true）
        clientId: 'ih9jy7J7gSn.F407|securemode=2,signmethod=hmacsha256,timestamp=1751705088349|',
        password: 'ba853a19a1ddc299f0bd03cac16aa556e2efe0835d34c61c362d82a95aa475d3',
        username: 'F407&ih9jy7J7gSn',
      },

      aliyunInfo: {
        productKey: 'ih9jy7J7gSn', //阿里云连接的三元组 ，请自己替代为自己的产品信息!!
        deviceName: 'F407', //阿里云连接的三元组 ，请自己替代为自己的产品信息!!
        deviceSecret: 'ad5f1c8d6ea6600695a75b356c1dd378', //阿里云连接的三元组 ，请自己替代为自己的产品信息!!
        regionId: 'cn-shanghai', //阿里云连接的三元组 ，请自己替代为自己的产品信息!!
        pubTopic: '/ih9jy7J7gSn/F407/user/get', //发布消息的主题
        subTopic: '/ih9jy7J7gSn/F407/user/get', //订阅消息的主题
      },
    },

  onLoad:function(){
    that = this;
    let clientOpt = aliyunOpt.getAliyunIotMqttClient({
      productKey: that.data.aliyunInfo.productKey,
      deviceName: that.data.aliyunInfo.deviceName,
      deviceSecret: that.data.aliyunInfo.deviceSecret,
      regionId: that.data.aliyunInfo.regionId,
      port: that.data.aliyunInfo.port,
    });

    console.log("get data:" + JSON.stringify(clientOpt));
    let host = 'wxs://' + clientOpt.host;
    
    this.setData({
      'options.clientId': clientOpt.clientId,
      'options.password': clientOpt.password,
      'options.username': clientOpt.username,
    })
    console.log("this.data.options host:" + host);
    console.log("this.data.options data:" + JSON.stringify(this.data.options));

    //访问服务器
    this.data.client = mqtt.connect(host, this.data.options);

    this.data.client.on('connect', function (connack) {
      console.log("连接成功");
    })

    //接收消息监听
    that.data.client.on("message", function (topic, payload) {
      //message是一个16进制的字节流
      let dataFromALY = {};
      try {
        dataFromALY = JSON.parse(payload.toString());
        console.log(dataFromALY);
       that.setData({
        //转换成JSON格式的数据进行读取
        LightSwitch:dataFromALY.LightSwitch,
        longitude:dataFromALY.longitude,
        latitude:dataFromALY.latitude,
      })
      } catch (error) {
        console.log(error);
      }
    })

    //服务器连接异常的回调
    that.data.client.on("error", function (error) {
      console.log(" 服务器 error 的回调" + error)

    })
    //服务器重连连接异常的回调
    that.data.client.on("reconnect", function () {
      console.log(" 服务器 reconnect的回调")

    })
    //服务器连接异常的回调
    that.data.client.on("offline", function (errr) {
      console.log(" 服务器offline的回调")
    })
  },
  
  onClickOpen() {
    that.sendCommond(1);
  },
  onClickOff() {
    that.sendCommond(0);
  },
  sendCommond(data) {
    let sendData = {
      LightSwitch: data,
      longitude: data,
      latitude: data,
    };

//此函数是订阅的函数，因为放在访问服务器的函数后面没法成功订阅topic，因此把他放在这个确保订阅topic的时候已成功连接服务器
//订阅消息函数，订阅一次即可 如果云端没有订阅的话，需要取消注释，等待成功连接服务器之后，在随便点击（开灯）或（关灯）就可以订阅函数
     this.data.client.subscribe(this.data.aliyunInfo.subTopic,function(err){
      if(!err){
        console.log("订阅成功");
      };
      wx.showModal({
        content: "订阅成功",
        showCancel: false,
      })
    })  
    

    //发布消息
    if (this.data.client && this.data.client.connected) {
      this.data.client.publish(this.data.aliyunInfo.pubTopic, JSON.stringify(sendData));
      console.log(this.data.aliyunInfo.pubTopic)
      console.log(JSON.stringify(sendData))
    } else {
      wx.showToast({
        title: '请先连接服务器',
        icon: 'none',
        duration: 2000
      })
    }
  }
})