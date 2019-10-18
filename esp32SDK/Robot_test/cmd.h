

typedef struct {
    uint8_t Value;//当前电平
    uint8_t Dir;//方向
    double ClkCnt; //速度
} WHEEL_INFO;

typedef struct {
    WHEEL_INFO LFront;//左前
    WHEEL_INFO RFront;//右前
    WHEEL_INFO LRear;//左后
    WHEEL_INFO RRear;//右后
} CAR_INFO;

typedef struct {
    uint32_t CTime;//当前电平
    uint32_t Speed;
} CHANNEL_INFO;

typedef struct {
    CHANNEL_INFO BAfter;//前后油门
    CHANNEL_INFO LRight; //左右油门
    CHANNEL_INFO UDown;//云台上下
    CHANNEL_INFO Rotating; //自转
} CONTROL_INFO;


