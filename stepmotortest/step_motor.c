#include <stdio.h>
#define STAGT_CNT 4
#define ACC_STEP_NUM 40
unsigned short AccStep[ACC_STEP_NUM] = {
909,885,855,820,781,735,685,633,575,515,
457,402,348,301,259,223,192,166,145,127,
113,102,93,86,80,76,72,70,68,66,
65,64,63,62,62,61,61,61,61,60,
};
#define INDEX_MAX (ACC_STEP_NUM-1)


enum {
    STATE_CUT1 = 0,
    STATE_UP    = 1,
    STATE_CON   = 2,
    STATE_CUT2 = 3,
};
enum {
    FINISH = 0,
    UN_FINISH    = 1,
};

typedef struct STInfo{
    char flag;//完成标志
	char dir;//方向
    int res;//剩余步数
    int index;//脚标位置
    int aimIndex;//完成index
    char lastDir;//最终方向
}t_StageInfo;

typedef struct FUInfo{
    int nowPlace;//当前位置
    char nowDir;//当前方向
    char lastDir;//最终方向
    int cutRes;//减速剩余步数/加速已用步数
    int totalStep;//完整曲线步数
    t_StageInfo StageInfo[STAGT_CNT];
}t_FullInfo;
t_FullInfo FullInfo;

/*属兴继承*/
void InheritInfo(int index){
    if (index > 0){//其他阶段继承上一属兴
        FullInfo.StageInfo[index].dir = FullInfo.StageInfo[index - 1].lastDir;
        FullInfo.StageInfo[index].index = FullInfo.StageInfo[index - 1].aimIndex;
        FullInfo.StageInfo[index].lastDir = FullInfo.StageInfo[index].dir;//默认下一阶段方向同向
        FullInfo.StageInfo[index].aimIndex = FullInfo.StageInfo[index].index;
    }
    else{//第一阶段继承当前运动属兴
        FullInfo.StageInfo[index].dir = FullInfo.nowDir;
        FullInfo.StageInfo[index].index = FullInfo.cutRes;
        FullInfo.StageInfo[index].lastDir = FullInfo.lastDir;
        FullInfo.StageInfo[index].aimIndex = FullInfo.StageInfo[index].index;
    }
}

/*属兴继承*/
void SetFlag(int index){
    FullInfo.StageInfo[index].flag = UN_FINISH;
}

void UpdateInfo(int ArmPlace){
    int resize;
    FullInfo.lastDir = 1;//默认正向
    for(int i=0; i++; i<STAGT_CNT){//初始化各阶段默认值
        FullInfo.StageInfo[i].res = 0;
        FullInfo.StageInfo[i].flag = FINISH;
        FullInfo.StageInfo[i].aimIndex = FullInfo.cutRes;//剩余减速步数正好对应加减曲线index
    }

    resize = ArmPlace - FullInfo.nowPlace;//计算调整值
    if (0 > resize){
        FullInfo.lastDir = 0;
        resize = resize * (-1);
    }
    printf("resize = %d,lastDir = %d\n",resize,FullInfo.lastDir);
    FullInfo.totalStep = resize + FullInfo.cutRes;//全程完成梯形曲线长度,有没有减速一样。

    //减1阶段
    InheritInfo(STATE_CUT1);
    if (((FullInfo.StageInfo[STATE_CUT1].lastDir == FullInfo.StageInfo[STATE_CUT1].dir) && (FullInfo.cutRes > resize)) || //同向且减速距离>差值（刹不住）
        ((FullInfo.StageInfo[STATE_CUT1].lastDir != FullInfo.StageInfo[STATE_CUT1].dir) && (0 != FullInfo.cutRes))){//异向且速度不是最小值
        FullInfo.StageInfo[STATE_CUT1].res = FullInfo.StageInfo[STATE_CUT1].index;
        FullInfo.StageInfo[STATE_CUT1].aimIndex = 0;//会减速到最低值
        FullInfo.StageInfo[STATE_CUT1].index = FullInfo.StageInfo[STATE_CUT1].index - 1;//修正index，比上一步减
        SetFlag(STATE_CUT1);//更新flag
    }
    printf("STATE_CUT1:flag:%d,index:%d,dir:%d,res:%d\n",FullInfo.StageInfo[STATE_CUT1].flag,FullInfo.StageInfo[STATE_CUT1].index,
    FullInfo.StageInfo[STATE_CUT1].dir,FullInfo.StageInfo[STATE_CUT1].res);
    //加速阶段
    InheritInfo(STATE_UP);
    if (FullInfo.totalStep/2 > FullInfo.StageInfo[STATE_CUT1].aimIndex){//有加速环节
        if (FullInfo.totalStep/2 >= INDEX_MAX){//可以到达最高速

            FullInfo.StageInfo[STATE_UP].res = INDEX_MAX - FullInfo.StageInfo[STATE_UP].index;
            FullInfo.StageInfo[STATE_UP].aimIndex = INDEX_MAX;//会加速到最高值
        }
        else{
            FullInfo.StageInfo[STATE_UP].res = FullInfo.totalStep/2 - FullInfo.StageInfo[STATE_UP].index;
            FullInfo.StageInfo[STATE_UP].aimIndex = FullInfo.totalStep/2;//会加速到
        }
        FullInfo.StageInfo[STATE_UP].index = FullInfo.StageInfo[STATE_UP].index + 1;//修正index，比上一步加
        SetFlag(STATE_UP);
    }
    printf("STATE_UP:flag:%d,index:%d,dir:%d,res:%d\n",FullInfo.StageInfo[STATE_UP].flag,FullInfo.StageInfo[STATE_UP].index,
    FullInfo.StageInfo[STATE_UP].dir,FullInfo.StageInfo[STATE_UP].res);
    //匀速阶段
    InheritInfo(STATE_CON);
    if (INDEX_MAX == FullInfo.StageInfo[STATE_CON].index){//最高速，说明有匀速阶段
        FullInfo.StageInfo[STATE_CON].res = FullInfo.totalStep - 2*INDEX_MAX;
        SetFlag(STATE_CON);
    }
    printf("STATE_CON:flag:%d,index:%d,dir:%d,res:%d\n",FullInfo.StageInfo[STATE_CON].flag,FullInfo.StageInfo[STATE_CON].index,
    FullInfo.StageInfo[STATE_CON].dir,FullInfo.StageInfo[STATE_CON].res);

    //减2阶段
    InheritInfo(STATE_CUT2);
    if (0 != FullInfo.totalStep){//只要不等于0就有减速阶段
        if (FullInfo.totalStep/2 >= INDEX_MAX){//可以到达最高速
            FullInfo.StageInfo[STATE_CUT2].res = INDEX_MAX;
        }
        else{
            FullInfo.StageInfo[STATE_CUT2].res = FullInfo.totalStep/2;
        }
        FullInfo.StageInfo[STATE_CUT2].aimIndex = 0;//会减速到最低值
        FullInfo.StageInfo[STATE_CUT2].index = FullInfo.StageInfo[STATE_CUT2].index - 1;//修正index，比上一步减
        SetFlag(STATE_CUT2);
    }
    printf("STATE_CUT2:flag:%d,index:%d,dir:%d,res:%d\n",FullInfo.StageInfo[STATE_CUT2].flag,FullInfo.StageInfo[STATE_CUT2].index,
    FullInfo.StageInfo[STATE_CUT2].dir,FullInfo.StageInfo[STATE_CUT2].res);

}

void TimerHandler(void){
    int tempT = 0;
    char Dir = 1;
    int index = 0;
    if (UN_FINISH == FullInfo.StageInfo[STATE_CUT1].flag){//减速到最低
        Dir = FullInfo.StageInfo[STATE_CUT1].dir;
        index = FullInfo.StageInfo[STATE_CUT1].index;
        if (FullInfo.StageInfo[STATE_CUT2].aimIndex < FullInfo.StageInfo[STATE_CUT1].index){
            FullInfo.StageInfo[STATE_CUT1].index --;
        }
        else{
            FullInfo.StageInfo[STATE_CUT1].flag = FINISH;
        }
    }
    else if (UN_FINISH == FullInfo.StageInfo[STATE_UP].flag){//加速到最高
        Dir = FullInfo.StageInfo[STATE_UP].dir;
        index = FullInfo.StageInfo[STATE_UP].index;
        if (FullInfo.StageInfo[STATE_UP].aimIndex > FullInfo.StageInfo[STATE_UP].index){
            FullInfo.StageInfo[STATE_UP].index ++;
        }
        else{
            FullInfo.StageInfo[STATE_UP].flag = FINISH;
        }
    }
    else if (UN_FINISH == FullInfo.StageInfo[STATE_CON].flag){//匀速阶段看剩余步数
        Dir = FullInfo.StageInfo[STATE_CON].dir;
        index = FullInfo.StageInfo[STATE_CON].index;
        if (1 < FullInfo.StageInfo[STATE_CON].res){//有几步，进几次
            FullInfo.StageInfo[STATE_CON].res --;
        }
        else{
            FullInfo.StageInfo[STATE_CON].flag = FINISH;
        }
    }
    else if (UN_FINISH == FullInfo.StageInfo[STATE_CUT2].flag){//减速到最低
        Dir = FullInfo.StageInfo[STATE_CUT2].dir;
        index = FullInfo.StageInfo[STATE_CUT2].index;
        if (FullInfo.StageInfo[STATE_CUT2].aimIndex < FullInfo.StageInfo[STATE_CUT2].index){
            FullInfo.StageInfo[STATE_CUT2].index --;
        }
        else{
            FullInfo.StageInfo[STATE_CUT2].flag = FINISH;
        }
    }
    if(Dir){
        FullInfo.nowPlace++;
    }
    else{
        FullInfo.nowPlace--;
    }
    FullInfo.nowDir = Dir;
    FullInfo.cutRes = index;
    printf("Dir = %d,Index = %d,NowPlace = %d\n",FullInfo.nowDir,FullInfo.cutRes,FullInfo.nowPlace);
}

int main(){
    int x=200;
    int AimPlace = 2920;
    FullInfo.nowDir = 1;
    FullInfo.nowPlace = 3000;
    FullInfo.cutRes = 10;
    UpdateInfo(AimPlace);

    while(x--){
        TimerHandler();
        if(FullInfo.nowPlace == AimPlace){
            break;
        }
    }
}

