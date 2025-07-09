
import * as echarts from '../../ec-canvas/echarts';
Page({
  data: {
    ec1: {
        onInit: null // 初始化为 null，稍后动态赋值
      },
    city: "加载中...",
    provice: '',
    temp: "--",
    icon: '',
    weather: "loading",
    realFeel: "--",
    humi: '346',
    km: '--',
    speed: '--',
    scle: '--',
    weekly: [],
    processedLifeIndex: []
  },

  onLoad() {
      console.log('onload');
      this.setData({
        ec1: {
            onInit: this.initChart1.bind(this) // 绑定 this
          },
      })
    this.getWeather();
  },
  initChart1(canvas, width, height, dpr) {
    console.log('init');
    return this.initChart_line1(canvas, width, height, dpr); // 第三个图表的数据为
  },
  initChart_line1(canvas, width, height, dpr) {
    // 初始化 ECharts 实例
    console.log('width3', width);
    console.log('height3', height);
    const chart = echarts.init(canvas, null, {
      width: width, // 设置图表宽度
      height: height, // 设置图表高度
      devicePixelRatio: dpr // 设置设备像素比
    });
    // 将图表实例绑定到 Canvas
    canvas.setChart(chart);

    // 配置折线图选项
    const option = {
      xAxis: {
        type: 'category',
        data: ['05.15', '05.16', '05.17', '05.18', '05.19', '05.20','05.21'],
        axisLine: {
          show: false
        },
        axisLabel: {
          show: true
        }
      },
      yAxis: {
        show: false
      },
      series: [
        {
          data: [28, 29, 27, 28, 29,30,29],
          type: 'line',
          smooth: true,
          showSymbol: true,
          label: {
            show: true,
            position: 'top',
            formatter: '{c}'
          }
        },
        {
          data: [10, 13, 12, 15, 11,20,15],
          type: 'line',
          smooth: true,
          showSymbol: true,
          label: {
            show: true,
            position: 'top',
            formatter: '{c}'
          }
        }
      ],
      grid: {
        top: '15%',
        left: '10%',
        right: '-1.5%',
        bottom: '10%',
        containLabel: true
      }
    };
    // 将配置项应用到图表中
    chart.setOption(option);
    // 返回图表实例
    return chart;
  },
  showRegionPicker() {
    wx.navigateTo({
      url: '/pages/search/search',
    })
  },

  // 新增函数：根据城市名称获取天气数据
  getWeatherByCity(cityName) {
    const that = this;
    const key = 'eb0c86513b134bd4a66b87e35b9f8f8a';
    
    wx.request({
      url: `https://nj6r6pwu2k.re.qweatherapi.com/geo/v2/city/lookup?location=${cityName}&key=${key}`,
      success(res) {
        const cityData = res.data.location[0];
        wx.setStorageSync('city', cityData);
        that.loadWeatherData();
      },
      fail() {
        wx.showToast({
          title: '获取城市信息失败',
          icon: 'none'
        });
      }
    });
  },
  onShow() {
    const currentCity = wx.getStorageSync('currentCity');
    if (currentCity) {
      this.getWeather(currentCity.lat, currentCity.lon, true);
      wx.removeStorageSync('currentCity');
    }
  },

  getWeather: function(latitude, longitude, isManual = false) {
    console.log('进入');
    const that = this;
    
    const getWeatherData = (lat, lon) => {
      const key='eb0c86513b134bd4a66b87e35b9f8f8a';
      
      // 获取城市信息
      wx.request({
        url: `https://nj6r6pwu2k.re.qweatherapi.com/geo/v2/city/lookup?location=${lon},${lat}&key=${key}`,
        success(res) {
          const cityData = res.data.location[0];
          wx.setStorageSync('city', cityData);
          
          // 获取实时天气
          wx.request({
            url: `https://nj6r6pwu2k.re.qweatherapi.com/v7/weather/now?location=${lon},${lat}&key=${key}`,
            success(res1) {
              const weatherData = res1.data.now;
              wx.setStorageSync('weather', weatherData);
              
              // 获取7天预报
              wx.request({
                url: `https://nj6r6pwu2k.re.qweatherapi.com/v7/weather/7d?location=${lon},${lat}&key=${key}`,
                success(res2) {
                  const weeklyData = res2.data.daily;
                  wx.setStorageSync('after_days', weeklyData);
                  
                  // 获取生活指数
                  wx.request({
                    url: `https://nj6r6pwu2k.re.qweatherapi.com/v7/indices/1d?type=1,8,3,5&location=${lon},${lat}&key=${key}`,
                    success(res3) {
                      const lifeIndexData = res3.data.daily;
                      wx.setStorageSync('indicate', lifeIndexData);
                      
                      that.loadWeatherData();
                    }
                  });
                }
              });
            }
          });
        },
        fail() {
          wx.showToast({
            title: isManual ? '获取城市信息失败' : '获取位置失败',
            icon: 'none'
          });
        }
      });
    };

    if (isManual) {
      // 手动选择城市时直接使用传入的经纬度
      getWeatherData(latitude, longitude);
    } else {
      // 自动定位获取经纬度
      wx.getLocation({
        type: 'wgs84',
        success(res) {
          getWeatherData(res.latitude, res.longitude);
        },
        fail() {
          wx.showToast({
            title: '获取位置失败',
            icon: 'none'
          });
        }
      });
    }
  },

  loadWeatherData() {
    const city = wx.getStorageSync('city') || {};
    const weather = wx.getStorageSync('weather') || {};
    const indicate = wx.getStorageSync('indicate') || [];
    const after_days = wx.getStorageSync('after_days') || [];
    
    // 处理未来天气数据
    const processedWeekly = after_days.map(item => {
      const dateObj = new Date(item.fxDate);
      const weekDays = ['周日', '周一', '周二', '周三', '周四', '周五', '周六'];
      return {
        ...item,
        week: weekDays[dateObj.getDay()],
        fxDate: item.fxDate.substring(5),
        tempMin: item.tempMin,
        tempMax: item.tempMax,
        iconDay: item.iconDay
      };
    });

    // 提取折线图数据
    const xAxisData = processedWeekly.map(item => item.fxDate);
    const minTempData = processedWeekly.map(item => item.tempMin);
    const maxTempData = processedWeekly.map(item => item.tempMax);
    
    const processedLifeIndex = indicate.slice(0, 4).map(item => ({
      name: item.name,
      category: item.category,
      text: item.text
    }));
    
    this.setData({
      city: city.adm2 || '未知城市',
      provice: city.adm1 || '',
      name:city.name || '',
      temp: weather.temp || '--',
      icon: weather.icon || '',
      realFeel: weather.feelsLike || '--',
      weather: weather.text || '--',
      km: weather.vis || '--',
      speed: weather.windSpeed || '--',
      scle: weather.windDir || '--',
      humi: weather.humidity || '--',
      weekly: processedWeekly,
      processedLifeIndex,
      xAxisData,  // 添加x轴数据
      minTempData,  // 添加最低温度数据
      maxTempData  // 添加最高温度数据
    });

    // 更新折线图
    this.refresh_line('#mychart-dom-pie1', {
      xAxisData: xAxisData,
      minTempData: minTempData,
      maxTempData: maxTempData
    });
  },
  refresh_line(chartId, data) {
    const ecComponent = this.selectComponent(chartId);
    if (ecComponent && ecComponent.chart) {
      const chart = ecComponent.chart;
      const option = {
        xAxis: {
          data: data.xAxisData
        },
        series: [
          {
            data: data.maxTempData
          },
          {
            data: data.minTempData
          }
        ]
      };
      chart.setOption(option);
    }
  },
});


