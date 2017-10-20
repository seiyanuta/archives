const msgpack = require('msgpack-lite')

const SMMS_VERSION_MSG = 1
const SMMS_DEVICE_ID_MSG = 0x0a
const SMMS_DEVICE_INFO_MSG = 0x0b
const SMMS_LOG_MSG = 0x0c
const SMMS_OS_VERSION_MSG = 0x10
const SMMS_APP_VERSION_MSG = 0x11
const SMMS_OS_UPDATE_REQUEST_MSG = 0x20
const SMMS_APP_UPDATE_REQUEST_MSG = 0x21
const SMMS_STORE_MSG = 0x40
const SMMS_STORE_MSG_END = 0x7f

class AdapterBase {
  constructor() {
    this.onReceiveCallback = () => { }
  }

  onReceive(callback) {
    this.onReceiveCallback = callback
  }

  serialize(messages) {
    const { state, osVersion, appVersion, log } = messages
    const states = { new: 1, booting: 2, ready: 3, running: 4, down: 5, reboot: 6, relaunch: 7 }

    if (!states[state]) {
      throw new Error(`Invalid device state: \`${state}'`)
    }

    let payload = {}
    payload[SMMS_VERSION_MSG] = 1
    payload[SMMS_DEVICE_INFO_MSG] = states[state]
    payload[SMMS_DEVICE_ID_MSG] = this.deviceId
    payload[SMMS_OS_VERSION_MSG] = osVersion
    payload[SMMS_APP_VERSION_MSG] = appVersion
    payload[SMMS_LOG_MSG] = log

    return msgpack.encode(payload)
  }

  deserialize(payload) {
    let stores = {}

    let data = msgpack.decode(payload)
    let osUpdateRequest = data[SMMS_OS_UPDATE_REQUEST_MSG]
    let appUpdateRequest = data[SMMS_APP_UPDATE_REQUEST_MSG]

    for (let k in data) {
      if (SMMS_STORE_MSG <= k && k < SMMS_STORE_MSG_END) { stores[data[k][0]] = data[k][1] }
    }

    return { osUpdateRequest, appUpdateRequest, stores }
  }
}

module.exports = AdapterBase
