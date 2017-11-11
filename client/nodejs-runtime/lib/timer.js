class TimerAPI {
  loop(interval, callback) {
    setInterval(callback, interval * 1000)
  }

  delay(duration, callback) {
    setTimeout(callback, duration * 1000)
  }

  sleep(duration) {
    return new Promise((resolve, reject) => {
      setTimeout(resolve, duration)
    })
  }

  busywait(usec) {
    if (Math.pow(10, 9) /* 1 sec */ <= usec) {
      throw new Error('Too long busy wait duration.')
    }

    const start = process.hrtime()

    while (true) {
      const [secDiff, nanosecDiff] = process.hrtime(start)
      if (secDiff > 0 || nanosecDiff > usec * 1000) {
        return
      }
    }
  }
}

module.exports = TimerAPI
