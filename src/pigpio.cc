#include <pigpio.h>
#include <nan.h>

#if NODE_VERSION_AT_LEAST(0, 11, 13)
static void gpioISREventLoopHandler(uv_async_t* handle);
#else
static void gpioISREventLoopHandler(uv_async_t* handle, int status);
#endif

// TODO errors returned by uv calls are ignored

class GpioISR_t {
public:
  GpioISR_t() : callback_(0) {
    uv_async_init(uv_default_loop(), &async_, gpioISREventLoopHandler);

    // Prevent async from keeping event loop alive, for the time being.
    uv_unref((uv_handle_t *) &async_);
  }

  ~GpioISR_t() {
    if (callback_) {
      uv_unref((uv_handle_t *) &async_);
      delete callback_;
    }

    uv_close((uv_handle_t *) &async_, 0);

    callback_ = 0;
  }

  void AsyncSend() {
    uv_async_send(&async_);
  }

  void SetCallback(Nan::Callback *callback) {
    if (callback_) {
      uv_unref((uv_handle_t *) &async_);
      delete callback_;
    }

    callback_ = callback;

    if (callback_) {
      uv_ref((uv_handle_t *) &async_);
    }
  }

  Nan::Callback *Callback() {
    return callback_;
  }

private:
  uv_async_t async_;
  Nan::Callback *callback_;
};


static GpioISR_t gpioISR_g[PI_MAX_USER_GPIO + 1];
static int gpio_g;
static int level_g;
static uint32_t tick_g;
static uv_sem_t sem_g;


void ThrowPigpioError(int err, const char *pigpiocall) {
  char buf[128];

  snprintf(buf, sizeof(buf), "pigpio error %d in %s", err, pigpiocall);

  Nan::ThrowError(buf);
}


NAN_METHOD(gpioInitialise) {
  int rc = gpioInitialise();
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioInitialise");
  }

  info.GetReturnValue().Set(rc);
}


NAN_METHOD(gpioTerminate) {
  gpioTerminate();
}


NAN_METHOD(gpioSetMode) {
  if (info.Length() < 2 || !info[0]->IsUint32() || !info[1]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioSetMode"));
  }

  unsigned gpio = info[0]->Uint32Value();
  unsigned mode = info[1]->Uint32Value();

  int rc = gpioSetMode(gpio, mode);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioSetMode");
  }
}


NAN_METHOD(gpioGetMode) {
  if (info.Length() < 1 || !info[0]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioGetMode"));
  }

  unsigned gpio = info[0]->Uint32Value();

  int rc = gpioGetMode(gpio);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioGetMode");
  }

  info.GetReturnValue().Set(rc);
}


NAN_METHOD(gpioSetPullUpDown) {
  if (info.Length() < 2 || !info[0]->IsUint32() || !info[1]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioSetPullUpDown"));
  }

  unsigned gpio = info[0]->Uint32Value();
  unsigned pud = info[1]->Uint32Value();

  int rc = gpioSetPullUpDown(gpio, pud);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioSetPullUpDown");
  }
}


NAN_METHOD(gpioRead) {
  if (info.Length() < 1 || !info[0]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioRead"));
  }

  unsigned gpio = info[0]->Uint32Value();

  int rc = gpioRead(gpio);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioRead");
  }

  info.GetReturnValue().Set(rc);
}


NAN_METHOD(gpioWrite) {
  if (info.Length() < 2 || !info[0]->IsUint32() || !info[1]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioSetPullUpDown"));
  }

  unsigned gpio = info[0]->Uint32Value();
  unsigned level = info[1]->Uint32Value();

  int rc = gpioWrite(gpio, level);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioWrite");
  }
}


NAN_METHOD(gpioPWM) {
  if (info.Length() < 2 || !info[0]->IsUint32() || !info[1]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioPWM"));
  }

  unsigned user_gpio = info[0]->Uint32Value();
  unsigned dutycycle = info[1]->Uint32Value();

  int rc = gpioPWM(user_gpio, dutycycle);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioPWM");
  }
}


NAN_METHOD(gpioHardwarePWM) {
  if (info.Length() < 3 ||
      !info[0]->IsUint32() ||
      !info[1]->IsUint32() ||
      !info[2]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioHardwarePWM"));
  }

  unsigned gpio = info[0]->Uint32Value();
  unsigned frequency = info[1]->Uint32Value();
  unsigned dutycycle = info[2]->Uint32Value();

  int rc = gpioHardwarePWM(gpio, frequency, dutycycle);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioHardwarePWM");
  }
}


NAN_METHOD(gpioGetPWMdutycycle) {
  if (info.Length() < 1 || !info[0]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioGetPWMdutycycle"));
  }

  unsigned user_gpio = info[0]->Uint32Value();

  int rc = gpioGetPWMdutycycle(user_gpio);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioGetPWMdutycycle");
  }

  info.GetReturnValue().Set(rc);
}


NAN_METHOD(gpioSetPWMrange) {
  if (info.Length() < 2 || !info[0]->IsUint32() || !info[1]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioSetPWMrange"));
  }

  unsigned user_gpio = info[0]->Uint32Value();
  unsigned range = info[1]->Uint32Value();

  int rc = gpioSetPWMrange(user_gpio, range);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioSetPWMrange");
  }
}


NAN_METHOD(gpioGetPWMrange) {
  if (info.Length() < 1 || !info[0]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioGetPWMrange"));
  }

  unsigned user_gpio = info[0]->Uint32Value();

  int rc = gpioGetPWMrange(user_gpio);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioGetPWMrange");
  }

  info.GetReturnValue().Set(rc);
}


NAN_METHOD(gpioGetPWMrealRange) {
  if (info.Length() < 1 || !info[0]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioGetPWMrealRange"));
  }

  unsigned user_gpio = info[0]->Uint32Value();

  int rc = gpioGetPWMrealRange(user_gpio);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioGetPWMrealRange");
  }

  info.GetReturnValue().Set(rc);
}


NAN_METHOD(gpioSetPWMfrequency) {
  if (info.Length() < 2 || !info[0]->IsUint32() || !info[1]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioSetPWMfrequency"));
  }

  unsigned user_gpio = info[0]->Uint32Value();
  unsigned frequency = info[1]->Uint32Value();

  int rc = gpioSetPWMfrequency(user_gpio, frequency);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioSetPWMfrequency");
  }
}


NAN_METHOD(gpioGetPWMfrequency) {
  if (info.Length() < 1 || !info[0]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioGetPWMfrequency"));
  }

  unsigned user_gpio = info[0]->Uint32Value();

  int rc = gpioGetPWMfrequency(user_gpio);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioGetPWMfrequency");
  }

  info.GetReturnValue().Set(rc);
}


NAN_METHOD(gpioServo) {
  if (info.Length() < 2 || !info[0]->IsUint32() || !info[1]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioServo"));
  }

  unsigned user_gpio = info[0]->Uint32Value();
  unsigned pulsewidth = info[1]->Uint32Value();

  int rc = gpioServo(user_gpio, pulsewidth);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioServo");
  }
}


NAN_METHOD(gpioGetServoPulsewidth) {
  if (info.Length() < 1 || !info[0]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioGetServoPulsewidth"));
  }

  unsigned user_gpio = info[0]->Uint32Value();

  int rc = gpioGetServoPulsewidth(user_gpio);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioGetServoPulsewidth");
  }

  info.GetReturnValue().Set(rc);
}


// gpioISRHandler is not executed in the event loop thread
static void gpioISRHandler(int gpio, int level, uint32_t tick) {
  uv_sem_wait(&sem_g);

  gpio_g = gpio;
  level_g = level;
  tick_g = tick;

  gpioISR_g[gpio].AsyncSend();
}


// gpioISREventLoopHandler is executed in the event loop thread.
#if NODE_VERSION_AT_LEAST(0, 11, 13)
static void gpioISREventLoopHandler(uv_async_t* handle) {
#else
static void gpioISREventLoopHandler(uv_async_t* handle, int status) {
#endif
  Nan::HandleScope scope;

  if (gpioISR_g[gpio_g].Callback()) {
    v8::Local<v8::Value> args[3] = {
      Nan::New<v8::Integer>(gpio_g),
      Nan::New<v8::Integer>(level_g),
      Nan::New<v8::Integer>(tick_g)
    };
    gpioISR_g[gpio_g].Callback()->Call(3, args);
  }

  uv_sem_post(&sem_g);
}


static NAN_METHOD(gpioSetISRFunc) {
  if (info.Length() < 3 ||
      !info[0]->IsUint32() ||
      !info[1]->IsUint32() ||
      !info[2]->IsInt32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioSetISRFunc"));
  }

  if (info.Length() >= 4 &&
      !info[3]->IsFunction() &&
      !info[3]->IsNull() &&
      !info[3]->IsUndefined()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioSetISRFunc"));
  }

  unsigned user_gpio = info[0]->Uint32Value();
  unsigned edge = info[1]->Uint32Value();
  int timeout = info[2]->Int32Value();
  Nan::Callback *callback = 0;
  gpioISRFunc_t isrFunc = 0;

  if (info.Length() >= 4 && info[3]->IsFunction()) {
    callback = new Nan::Callback(info[3].As<v8::Function>());
    isrFunc = gpioISRHandler;
  }

  gpioISR_g[user_gpio].SetCallback(callback);

  int rc = gpioSetISRFunc(user_gpio, edge, timeout, isrFunc);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioSetISRFunc");
  }
}


NAN_METHOD(gpioNotifyOpen) {
  int rc = gpioNotifyOpen();
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioNotifyOpen");
  }

  info.GetReturnValue().Set(rc);
}


NAN_METHOD(gpioNotifyOpenWithSize) {
  if (info.Length() < 1 || !info[0]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioNotifyOpenWithSize"));
  }

  unsigned bufSize = info[0]->Uint32Value();

  int rc = gpioNotifyOpenWithSize(bufSize);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioNotifyOpen");
  }

  info.GetReturnValue().Set(rc);
}


NAN_METHOD(gpioNotifyBegin) {
  if (info.Length() < 2 || !info[0]->IsUint32() || !info[1]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioNotifyBegin"));
  }

  unsigned handle = info[0]->Uint32Value();
  unsigned bits = info[1]->Uint32Value();

  int rc = gpioNotifyBegin(handle, bits);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioNotifyBegin");
  }
}


NAN_METHOD(gpioNotifyPause) {
  if (info.Length() < 1 || !info[0]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioNotifyPause"));
  }

  unsigned handle = info[0]->Uint32Value();

  int rc = gpioNotifyPause(handle);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioNotifyPause");
  }
}


NAN_METHOD(gpioNotifyClose) {
  if (info.Length() < 1 || !info[0]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioNotifyClose"));
  }

  unsigned handle = info[0]->Uint32Value();

  int rc = gpioNotifyClose(handle);
  if (rc < 0) {
    return ThrowPigpioError(rc, "gpioNotifyClose");
  }
}


NAN_METHOD(gpioCfgClock) {
  if (info.Length() < 2 || !info[0]->IsUint32() || !info[1]->IsUint32()) {
    return Nan::ThrowError(Nan::ErrnoException(EINVAL, "gpioCfgClock"));
  }

  unsigned cfgMicros = info[0]->Uint32Value();
  unsigned cfgPeripheral = info[0]->Uint32Value();
  unsigned cfgSource = 0;

  gpioCfgClock(cfgMicros, cfgPeripheral, cfgSource);
}


/*static void SetConst(
  Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target,
  const char* name,
  int value
) {
  Nan::Set(target,
    Nan::New<v8::String>(name).ToLocalChecked(),
    Nan::New<v8::Integer>(value)
  );
}*/


static void SetFunction(
  Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target,
  const char* name,
  Nan::FunctionCallback callback
) {
  Nan::Set(target,
    Nan::New(name).ToLocalChecked(),
    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(callback)).ToLocalChecked()
  );
}


NAN_MODULE_INIT(InitAll) {
  uv_sem_init(&sem_g, 1);

  /* mode constants */
/*  SetConst(target, "PI_INPUT", PI_INPUT);
  SetConst(target, "PI_OUTPUT", PI_OUTPUT);
  SetConst(target, "PI_ALT0", PI_ALT0);
  SetConst(target, "PI_ALT1", PI_ALT1);
  SetConst(target, "PI_ALT2", PI_ALT2);
  SetConst(target, "PI_ALT3", PI_ALT3);
  SetConst(target, "PI_ALT4", PI_ALT4);
  SetConst(target, "PI_ALT5", PI_ALT5);*/

  /* pud constants */
/*  SetConst(target, "PI_PUD_OFF", PI_PUD_OFF);
  SetConst(target, "PI_PUD_DOWN", PI_PUD_DOWN);
  SetConst(target, "PI_PUD_UP", PI_PUD_UP);*/

  /* isr constants */
/*  SetConst(target, "RISING_EDGE", RISING_EDGE);
  SetConst(target, "FALLING_EDGE", FALLING_EDGE);
  SetConst(target, "EITHER_EDGE", EITHER_EDGE);*/

  /* timeout constant */
/*  SetConst(target, "PI_TIMEOUT", PI_TIMEOUT); */

  /* gpio number constants */
/*  SetConst(target, "PI_MIN_GPIO", PI_MIN_GPIO);
  SetConst(target, "PI_MAX_GPIO", PI_MAX_GPIO);
  SetConst(target, "PI_MAX_USER_GPIO", PI_MAX_USER_GPIO);*/

  /* error code constants */
/*  SetConst(target, "PI_INIT_FAILED", PI_INIT_FAILED);*/

  /* gpioCfgClock cfgPeripheral constants */
/*  SetConst(target, "PI_CLOCK_PWM", PI_CLOCK_PWM);
  SetConst(target, "PI_CLOCK_PCM", PI_CLOCK_PCM);*/

  /* functions */
  SetFunction(target, "gpioInitialise", gpioInitialise);
  SetFunction(target, "gpioTerminate", gpioTerminate);

  SetFunction(target, "gpioSetMode", gpioSetMode);
  SetFunction(target, "gpioGetMode", gpioGetMode);
  SetFunction(target, "gpioSetPullUpDown", gpioSetPullUpDown);
  SetFunction(target, "gpioRead", gpioRead);
  SetFunction(target, "gpioWrite", gpioWrite);

  SetFunction(target, "gpioPWM", gpioPWM);
  SetFunction(target, "gpioHardwarePWM", gpioHardwarePWM);
  SetFunction(target, "gpioGetPWMdutycycle", gpioGetPWMdutycycle);
  SetFunction(target, "gpioSetPWMrange", gpioSetPWMrange);
  SetFunction(target, "gpioGetPWMrange", gpioGetPWMrange);
  SetFunction(target, "gpioGetPWMrealRange", gpioGetPWMrealRange);
  SetFunction(target, "gpioSetPWMfrequency", gpioSetPWMfrequency);
  SetFunction(target, "gpioGetPWMfrequency", gpioGetPWMfrequency);

  SetFunction(target, "gpioServo", gpioServo);
  SetFunction(target, "gpioGetServoPulsewidth", gpioGetServoPulsewidth);

  SetFunction(target, "gpioSetISRFunc", gpioSetISRFunc);

  SetFunction(target, "gpioNotifyOpen", gpioNotifyOpen);
  SetFunction(target, "gpioNotifyOpenWithSize", gpioNotifyOpenWithSize);
  SetFunction(target, "gpioNotifyBegin", gpioNotifyBegin);
  SetFunction(target, "gpioNotifyPause", gpioNotifyPause);
  SetFunction(target, "gpioNotifyClose", gpioNotifyClose);

  SetFunction(target, "gpioCfgClock", gpioCfgClock);
}

NODE_MODULE(pigpio, InitAll)

