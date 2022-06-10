#include "iec_base.h"
#include <QDebug>

using namespace std;

iec_base::iec_base() {
    this->slavePort = SERVERPORT;  // set iec104 tcp port to 2404
    strncpy(this->slaveIP, "", 20);
    this->masterAddr = 1;  // originator address
    this->slaveAddr = 0;   // common address of ASDU

    this->vs = 0;
    this->vr = 0;
    this->t0Timeout = -1;
    this->t1Timeout = -1;
    this->t2Timeout = -1;
    this->t3Timeout = -1;
    this->isConnected = false;
    this->ifCheckSeq = true;
    this->allowSend = true;

    this->numMsgUnack = 0;
    this->numMsgReceived = 0;

    initClient();
}

uint32_t iec_base::getSlavePort() {
    return this->slavePort;
}

void iec_base::setSlavePort(uint32_t port) {
    this->slavePort = port;
}

char* iec_base::getSlaveIP() {
    return this->slaveIP;
}

void iec_base::setSlaveIP(const char* ip) {
    strncpy(this->slaveIP, ip, 20);
}

uint16_t iec_base::getSlaveAddr() {
    return this->slaveAddr;
}

void iec_base::setSlaveAddr(uint16_t addr) {
    this->slaveAddr = addr;
}

void iec_base::initClient() {
    // init umapType2String
    this->umapType2String.clear();
    this->umapType2String[1] = "M_SP_NA_1";
    this->umapType2String[2] = "M_SP_TA_1";
    this->umapType2String[3] = "M_DP_NA_1";
    this->umapType2String[5] = "M_ST_NA_1";
    this->umapType2String[6] = "M_ST_TA_1";
    this->umapType2String[7] = "M_BO_NA_1";
    this->umapType2String[8] = "M_BO_TA_1";
    this->umapType2String[9] = "M_ME_NA_1";
    this->umapType2String[11] = "M_ME_NB_1";
    this->umapType2String[12] = "";
    this->umapType2String[13] = "M_ME_NC_1";
    this->umapType2String[14] = "";
    this->umapType2String[15] = "M_IT_NA_1";
    this->umapType2String[16] = "";
    this->umapType2String[17] = "";
    this->umapType2String[18] = "";
    this->umapType2String[19] = "";
    this->umapType2String[20] = "M_PS_NA_1";
    this->umapType2String[30] = "M_SP_TB_1";
    this->umapType2String[31] = "M_DP_TB_1";
    this->umapType2String[32] = "M_ST_TB_1";
    this->umapType2String[33] = "M_BO_TB_1";
    this->umapType2String[34] = "M_ME_TD_1";
    this->umapType2String[35] = "M_ME_TE_1";
    this->umapType2String[36] = "M_ME_TF_1";
    this->umapType2String[37] = "M_IT_TB_1";
    this->umapType2String[38] = "M_EP_TD_1";
    this->umapType2String[39] = "M_EP_TE_1";
    this->umapType2String[40] = "M_EP_TF_1";
    this->umapType2String[45] = "C_SC_NA_1:single command";    //
    this->umapType2String[46] = "C_DC_NA_1:double command";
    this->umapType2String[47] = "C_RC_NA_1:regulating step command";
    this->umapType2String[48] = "C_SE_NA_1:set-point normalised command";
    this->umapType2String[49] = "C_SE_NB_1:set-point scaled command";
    this->umapType2String[50] = "C_SE_NC_1:set-point short floating point command";
    this->umapType2String[51] = "C_BO_NA_1:Bitstring of 32 bit command";
    this->umapType2String[58] = "C_SC_TA_1:single command with time tag";
    this->umapType2String[59] = "C_DC_TA_1:double command with time tag";
    this->umapType2String[60] = "C_RC_TA_1:regulating step command with time tag";
    this->umapType2String[61] = "C_SE_TA_1:set-point normalised command with time tag";
    this->umapType2String[62] = "C_SE_TB_1:set-point scaled command with time tag";
    this->umapType2String[63] = "C_SE_TC_1:set-point short floating point command with time tag";
    this->umapType2String[64] = "C_BO_TA_1:Bitstring of 32 bit command with time tag";
    this->umapType2String[70] = "M_EI_NA_1:end of initialization";
    this->umapType2String[100] = "C_IC_NA_1:general interrogation";
    this->umapType2String[101] = "C_CI_NA_1:counter interrogation";
    this->umapType2String[102] = "C_RD_NA_1:read command";
    this->umapType2String[103] = "C_CS_NA_1:clock synchronization command";
    this->umapType2String[105] = "C_RP_NA_1:reset process command";
    this->umapType2String[107] = "C_TS_TA_1:test command with time tag CP56Time2a";
    this->umapType2String[110] = "P_ME_NA_1:Parameter of measured values, normalized value";
    this->umapType2String[111] = "P_ME_NB_1:Parameter of measured values, scaled value";
    this->umapType2String[112] = "P_ME_NC_1:Parameter of measured values, short floating point number";
    this->umapType2String[113] = "P_AC_NA_1:Parameter activation";
}

/**
 * @brief iec_base::messageReadyRead - tcp messages are ready to be read from the
 * connection with slave station.
 */
void iec_base::messageReadyRead() {
    static bool brokenMsg = false;
    static apdu wapdu;
    uint8_t* p;
    p = (uint8_t*)&wapdu;
    uint32_t bytesNum /* result of readTCP */, len /* apdu length */;
    char buf[1000];

    while (true) {
        if (!brokenMsg) {
            // 找到START
            do {
                bytesNum = readTCP((char*)p, 1);
                if (bytesNum == 0) {
                    return;  // 该函数被调用说明这里不应该被执行到
                }
            } while (p[0] != START);

            // 找到len
            bytesNum = readTCP((char*)p + 1, 1);
            if (bytesNum == 0) {
                return;
            }
        }

        len = p[1];
        if (len < 4) {  // 小于4说明是错误的帧，重新寻找
            brokenMsg = false;
            log.pushMsg("ERROR: invalid frame");
            continue;
        }

        // 读取除了68和length剩下的部分
        waitForReadyRead(len, 300);  // 等待len字节的数据被准备好
        bytesNum = readTCP((char*)p + 2, len);
        if (bytesNum == 0) {
            log.pushMsg("ERROR: broken msg");
            brokenMsg = true;
            return;
        } else if (bytesNum < len) {
            uint32_t rest = len - bytesNum;
            sprintf(buf, "--> should reread %d(%d in total)", rest, len);
            log.pushMsg(buf);

            // 第二次读
            waitForReadyRead(rest, 300);
            bytesNum = readTCP((char*)p + 2 + bytesNum, rest);
            sprintf(buf, "--> reread %d bytes in fact", bytesNum);
            if (bytesNum < rest) {
                log.pushMsg("--> broken msg");
                brokenMsg = true;
                return;
            }
        }

        brokenMsg = false;
        showMessage((const char*)p, len + 2, false);
        parse(&wapdu, wapdu.length + 2);
        break;
    }
}

void iec_base::showMessage(const char* buf, int size, bool isSend) {
    char buffer[200];

    if (log.isLogging()) {
        memset(buffer, 0, sizeof(buffer));
        if (isSend) {
            sprintf(buffer, "send    --> size: (%03d) ", size);
        } else {
            sprintf(buffer, "receive --> size: (%03d) ", size);
        }
        int cnt = 20, i;
        for (i = 0; i < size && i < cnt; ++i) {
            sprintf(buffer + strlen(buffer), "%02x ", (uint8_t)buf[i]);
        }
        if (size > cnt) {
            sprintf(buffer + strlen(buffer), "...");
        }
        log.pushMsg(buffer);
    }
}

void iec_base::send(const struct apdu& wapdu) {
    //    this->sendTCP(reinterpret_cast<const char*>(&wapdu), wapdu.length +
    //    2);
    this->sendTCP(reinterpret_cast<const char*>(&wapdu),
                  wapdu.length + sizeof(wapdu.length) + sizeof(wapdu.start));
}

void iec_base::onTcpConnect() {
    this->vr = 0;
    this->vs = 0;
    this->isConnected = true;
    // NOTE: 按照104规约，通信链路建立后应该立即发送U帧连接请求，我们将该功能由用户决定
    //    sendStartDtAct();  // 请求建立通信链路(主站->从站)
    log.pushMsg("connect success!");
}

void iec_base::onTcpDisconnect() {
    this->isConnected = false;
    log.pushMsg("disconnect success!");
    // TODO: 超时控制
    this->t1Timeout = -1;
    this->t2Timeout = -1;
    this->vs = this->vr = 0;
    this->isConnected = false;
    this->numMsgReceived = 0;
    this->numMsgReceived = 0;
}

/**
 * @brief iec_base::onTimeoutPerSecond - when the timer is up and modify the
 * timeout.
 */
void iec_base::onTimeoutPerSecond() {
    if (isConnected) {
        if (t1Timeout > 0) {
            --t1Timeout;
            if (t1Timeout == 0) {  // 超时，重新连接
                sendStartDtAct();  // t1Timeout < 0表示不需要处理
            }
        }

        if (t2Timeout > 0) {
            --t2Timeout;
            if (t2Timeout == 0) {
                sendMonitorMessage();
                t2Timeout = -1;
            }
        }

        if (t3Timeout > 0) {
            --t3Timeout;
            if (t3Timeout == 0) {
                sendTestfrAct();
            }
        }
    }
    // TODO:
}

/**
 * @brief iec_base::sendStartDtAct - send STARTDTACT from master to slave(68 04
 * 07 00 00 00)
 *
 */
void iec_base::sendStartDtAct() {
    struct apdu wapdu;

    wapdu.start = START;
    wapdu.length = 4;
    wapdu.NS = uint16_t(STARTDTACT);
    wapdu.NR = 0;
    send(wapdu);
    log.pushMsg("INFO: U(STARTDTACT)");

    t1Timeout = T1;  // reset timeout t1
}

void iec_base::sendStopDtAct() {
    struct apdu wapdu;
    wapdu.start = START;
    wapdu.length = 4;
    wapdu.NS = uint16_t(STOPDTACT);
    wapdu.NR = 0;
    send(wapdu);
    log.pushMsg("INFO: U(STOPDTACT)");
}

void iec_base::sendStopDtCon() {
    struct apdu wapdu;
    wapdu.start = START;
    wapdu.length = 4;
    wapdu.NS = uint16_t(STOPDTCON);
    wapdu.NR = 0;
    send(wapdu);
    log.pushMsg("INFO: U(STOPDTCON)");
}

void iec_base::sendTestfrAct() {
    struct apdu wapdu;
    wapdu.start = START;
    wapdu.length = 4;
    wapdu.NS = uint16_t(TESTFRACT);
    wapdu.NR = 0;
    send(wapdu);
    log.pushMsg("INFO: U(TESTFRACT)");
}

void iec_base::sendTestfrCon() {
    struct apdu wapdu;
    wapdu.start = START;
    wapdu.length = 4;
    wapdu.NS = uint16_t(TESTFRCON);
    wapdu.NR = 0;
    send(wapdu);
    log.pushMsg("INFO: U(TESTFRCON)");
}

void iec_base::sendMonitorMessage() {
    struct apdu wapdu;
    wapdu.start = START;
    wapdu.length = 4;
    wapdu.NS = SUPERVISORY;
    wapdu.NR = vr << 1;
    send(wapdu);
    char buf[100];
    sprintf(buf, "INFO: S(SUPERVISORY), the N(R) is %d", wapdu.NR);
    log.pushMsg(buf);
}

void iec_base::generalInterrogationAct() {
    stringstream ss;
    struct apdu a;
    a.start = START;
    a.length = 0x0E;
    a.NS = vs << 1;
    a.NR = vr << 1;
    a.head.type = INTERROGATION;
    a.head.num = 1;
    a.head.sq = 0;
    a.head.cot = ACTIVATION;  // 6: 激活（控制方向）
    //    a.head.t = 0;
    //    a.head.pn = 0;
    a.head.oa = masterAddr;
    a.head.ca = slaveAddr;
    a.nsq100.ioa16 = 0x00;
    a.nsq100.ioa8 = 0x00;
    a.nsq100.obj.qoi = 0x14;

    ss.str("");
    if (allowSend) {
        send(a);
        ss << "INFO: General Interrogation Activation";
        ++vs;
        ++numMsgUnack;
        if (numMsgUnack == CLIENTK) {
            allowSend = false;
        }
    } else {
        ss.str("");
        ss << "WARNING: 已达到最大未确认报文数k(" << CLIENTK
           << "), 停止报文发送";
        log.pushMsg(ss.str().c_str());
    }
}

/**
 * @brief iec_base::sendCommand - Send command to slave station.
 * @return
 */
bool iec_base::sendCommand(struct iec_obj* obj) {
    struct apdu cmd;
    stringstream ss;
    time_t t = time(nullptr);
    struct tm* timeinfo = localtime(&t);

    if (!allowSend) {
        ss.str("");
        ss << "WARNING: 已达到最大未确认报文数k(" << CLIENTK
           << "), 停止报文发送";
        log.pushMsg(ss.str().c_str());
        return false;
    }

    ss.str("");
    ss << "INFO: command " << umapType2String[obj->type].c_str() << "\n    ";
    switch (obj->type) {
    case C_SC_NA_1: {  // 45: single command
        cmd.start = START;
        cmd.length = sizeof(cmd.NR) + sizeof(cmd.NS) + sizeof(cmd.head) +
                sizeof(cmd.nsq45);
        cmd.NS = vs << 1;
        cmd.NR = vr << 1;
        cmd.head.type = obj->type;
        cmd.head.num = 1;  // 单信息元素
        cmd.head.sq = 0;   // sq = 0
        cmd.head.cot = obj->cause;
        cmd.head.pn = 0;
        cmd.head.t = 0;
        cmd.head.oa = masterAddr;
        cmd.head.ca = obj->ca;
        cmd.nsq45.ioa16 = (obj->address & 0xFFFF);
        cmd.nsq45.ioa8 = (obj->address >> 16);
        cmd.nsq45.obj.scs = obj->scs;
        cmd.nsq45.obj.res = 0;
        cmd.nsq45.obj.qu = obj->qu;
        cmd.nsq45.obj.se = obj->se;
        send(cmd);
        ++vs;
        if (log.isLogging()) {
            ss << "ADDRESS: " << (uint32_t)obj->address
               << ", TYPE: " << (uint32_t)obj->type
               << ", SCS: " << (uint32_t)obj->scs
               << ", QU: " << (uint32_t)obj->qu
               << ", SE: " << (uint32_t)obj->se
               << ", COT: " << (uint32_t)obj->cause;
            log.pushMsg(ss.str().c_str());
        }
    } break;

    case C_DC_NA_1: {  // 46: double command
        cmd.start = START;
        cmd.length = sizeof(cmd.NR) + sizeof(cmd.NS) + sizeof(cmd.head) +
                sizeof(cmd.nsq46);
        cmd.NS = vs << 1;
        cmd.NR = vr << 1;
        cmd.head.type = obj->type;
        cmd.head.num = 1;  // 单信息元素
        cmd.head.sq = 0;   // sq = 0
        cmd.head.cot = obj->cause;
        cmd.head.pn = 0;
        cmd.head.t = 0;
        cmd.head.oa = masterAddr;
        cmd.head.ca = obj->ca;
        cmd.nsq46.ioa16 = (obj->address & 0xFFFF);
        cmd.nsq46.ioa8 = (obj->address >> 16);
        cmd.nsq46.obj.dcs = obj->dcs;
        cmd.nsq46.obj.qu = obj->qu;
        cmd.nsq46.obj.se = obj->se;

        send(cmd);
        ++vs;
        if (log.isLogging()) {
            ss << "ADDRESS" << (uint32_t)obj->address
               << ", TYPE: " << (uint32_t)obj->type
               << ", DCS: " << (uint32_t)obj->dcs
               << ", QU: " << (uint32_t)obj->qu
               << ", SE: " << (uint32_t)obj->se
               << ", COT: " << (uint32_t)obj->cause;
            log.pushMsg(ss.str().c_str());
        }
    } break;

    case C_RC_NA_1: {  // 47: step adjustment command
        cmd.start = START;
        cmd.length = sizeof(cmd.NR) + sizeof(cmd.NS) + sizeof(cmd.head) +
                sizeof(cmd.nsq47);
        cmd.NS = vs << 1;
        cmd.NR = vr << 1;
        cmd.head.type = obj->type;
        cmd.head.num = 1;  // 单信息元素
        cmd.head.sq = 0;   // sq = 0
        cmd.head.cot = obj->cause;
        cmd.head.pn = 0;
        cmd.head.t = 0;
        cmd.head.oa = masterAddr;
        cmd.head.ca = obj->ca;
        cmd.nsq47.ioa16 = (obj->address & 0xFFFF);
        cmd.nsq47.ioa8 = (obj->address >> 16);
        cmd.nsq47.obj.rcs = obj->rcs;
        cmd.nsq47.obj.qu = obj->qu;
        cmd.nsq47.obj.se = obj->se;

        send(cmd);
        ++vs;
        if (log.isLogging()) {
            ss << "ADDRESS" << (uint32_t)obj->address
               << ", TYPE: " << (uint32_t)obj->type
               << ", RCS: " << (uint32_t)obj->rcs
               << ", QU: " << (uint32_t)obj->qu
               << ", SE: " << (uint32_t)obj->se
               << ", COT: " << (uint32_t)obj->cause;
            log.pushMsg(ss.str().c_str());
        }
    } break;

    case C_SE_NA_1: {  // 48: 设定命令, 规一化值
        cmd.start = START;
        cmd.length = sizeof(cmd.NR) + sizeof(cmd.NS) + sizeof(cmd.head) +
                sizeof(cmd.nsq48);
        cmd.NS = vs << 1;
        cmd.NR = vr << 1;
        cmd.head.type = obj->type;
        cmd.head.num = 1;  // 单信息元素
        cmd.head.sq = 0;   // sq = 0
        cmd.head.cot = obj->cause;
        cmd.head.pn = 0;
        cmd.head.t = 0;
        cmd.head.oa = masterAddr;
        cmd.head.ca = obj->ca;
        cmd.nsq48.ioa16 = (obj->address & 0xFFFF);
        cmd.nsq48.ioa8 = (obj->address >> 16);

        cmd.nsq48.obj.nva = obj->value;
        cmd.nsq48.obj.ql = 0;  // 默认为0
        cmd.nsq48.obj.se = obj->se;

        send(cmd);
        ++vs;
        if (log.isLogging()) {
            ss << "ADDRESS" << (uint32_t)obj->address
               << ", TYPE: " << (uint32_t)obj->type
               << ", RCS: " << (uint32_t)obj->rcs
               << ", QU: " << (uint32_t)obj->qu
               << ", SE: " << (uint32_t)obj->se
               << ", COT: " << (uint32_t)obj->cause;
            log.pushMsg(ss.str().c_str());
        }
    } break;

    case C_SC_TA_1: {  // 58: single command with cp56time2a
        cmd.start = START;
        cmd.length = sizeof(cmd.NR) + sizeof(cmd.NS) + sizeof(cmd.head) +
                sizeof(cmd.nsq58);
        cmd.NS = vs << 1;
        cmd.NR = vr << 1;
        cmd.head.type = obj->type;
        cmd.head.num = 1;  // 单信息元素
        cmd.head.sq = 0;   // sq = 0
        cmd.head.cot = obj->cause;
        cmd.head.pn = 0;
        cmd.head.t = 0;
        cmd.head.oa = masterAddr;
        cmd.head.ca = obj->ca;
        cmd.nsq58.ioa16 = (obj->address & 0xFFFF);
        cmd.nsq58.ioa8 = (obj->address >> 16);
        cmd.nsq58.obj.scs = obj->scs;
        cmd.nsq58.obj.res = 0;
        cmd.nsq58.obj.qu = obj->qu;
        cmd.nsq58.obj.se = obj->se;

        cmd.nsq58.obj.time.year = timeinfo->tm_year % 100;  // [0...99]
        cmd.nsq58.obj.time.mon = timeinfo->tm_mon + 1;
        cmd.nsq58.obj.time.dmon = timeinfo->tm_mday;
        cmd.nsq58.obj.time.dweek = timeinfo->tm_wday;
        cmd.nsq58.obj.time.hour = timeinfo->tm_hour;
        cmd.nsq58.obj.time.min = timeinfo->tm_min;
        cmd.nsq58.obj.time.mesc = timeinfo->tm_sec * 1000;
        cmd.nsq58.obj.time.res1 = cmd.nsq58.obj.time.res2 =
                cmd.nsq58.obj.time.res3 = cmd.nsq58.obj.time.res4 = 0;
        cmd.nsq58.obj.time.iv = 0;
        cmd.nsq58.obj.time.su = 0;

        send(cmd);
        ++vs;
        if (log.isLogging()) {
            ss << "ADDRESS" << (uint32_t)obj->address
               << ", TYPE: " << (uint32_t)obj->type
               << ", SCS: " << (uint32_t)obj->scs
               << ", QU: " << (uint32_t)obj->qu
               << ", SE: " << (uint32_t)obj->se
               << ", COT: " << (uint32_t)obj->cause;

            ss << "\n    TIME.YEAR" << (uint32_t)cmd.nsq58.obj.time.year  // 年 2022
               << ", TIME.MONTH" << (uint32_t)cmd.nsq58.obj.time.mon      // 月 5
               << ", TIME.DAY" << (uint32_t)cmd.nsq58.obj.time.dmon       // 日 24
               << ", TIME.HOUR" << (uint32_t)cmd.nsq58.obj.time.hour      // 时 14
               << ", TIME.MIN" << (uint32_t)cmd.nsq58.obj.time.min        // 分 53
               << ", TIME.SRC" << (uint32_t)timeinfo->tm_sec;             // 秒 21
            log.pushMsg(ss.str().c_str());
        }
    } break;

    case C_CS_NA_1: {  // 103: 时钟同步 clock syncchronization
        cmd.start = START;
        cmd.length = sizeof(cmd.NR) + sizeof(cmd.NS) + sizeof(cmd.head) +
                sizeof(cmd.nsq103);
        cmd.NS = vs << 1;
        cmd.NR = vr << 1;
        cmd.head.type = obj->type;
        cmd.head.num = 1;           // 单信息元素
        cmd.head.sq = 0;            // sq = 0
        cmd.head.cot = ACTIVATION;  // 控制方向传送原因=激活
        cmd.head.pn = 0;
        cmd.head.t = 0;
        cmd.head.oa = masterAddr;
        cmd.head.ca = obj->ca;
        cmd.nsq103.ioa16 = (obj->address & 0xFFFF);
        cmd.nsq103.ioa8 = (obj->address >> 16);

        cmd.nsq103.obj.time.year = timeinfo->tm_year % 100;  // [0...99]
        cmd.nsq103.obj.time.mon = timeinfo->tm_mon + 1;
        cmd.nsq103.obj.time.dmon = timeinfo->tm_mday;
        cmd.nsq103.obj.time.dweek = timeinfo->tm_wday;
        cmd.nsq103.obj.time.hour = timeinfo->tm_hour;
        cmd.nsq103.obj.time.min = timeinfo->tm_min;
        cmd.nsq103.obj.time.mesc = timeinfo->tm_sec * 1000;
        cmd.nsq103.obj.time.res1 = cmd.nsq103.obj.time.res2 =
                cmd.nsq103.obj.time.res3 = cmd.nsq103.obj.time.res4 = 0;
        cmd.nsq103.obj.time.iv = 0;
        cmd.nsq103.obj.time.su = 0;

        send(cmd);
        ++vs;
        if (log.isLogging()) {
            ss << "ADDRESS" << (uint32_t)obj->address
               << ", TYPE: " << (uint32_t)obj->type
               << ", COT: " << (uint32_t)cmd.head.cot;

            ss << "\n    TIME.YEAR" << cmd.nsq103.obj.time.year  // 年 2022
               << ", TIME.MONTH" << cmd.nsq103.obj.time.mon      // 月 5
               << ", TIME.DAY" << cmd.nsq103.obj.time.dmon       // 日 24
               << ", TIME.HOUR" << cmd.nsq103.obj.time.hour      // 时 14
               << ", TIME.MIN" << cmd.nsq103.obj.time.min        // 分 53
               << ", TIME.SRC" << timeinfo->tm_sec;              // 秒 21
            log.pushMsg(ss.str().c_str());
        }
    } break;

    default:
        return false;
        break;
    }

    ++numMsgUnack;
    if (numMsgUnack == CLIENTK) {
        allowSend = false;
    }

    return true;
}

/**
 * @brief iec_base::parse
 * @param papdu
 * @param size  - apdu.head.len + 2
 * @param isSend - If we want to send the result to server.
 */
void iec_base::parse(struct apdu* papdu, int size, bool isSend) {
    uint16_t vrReceived;
    stringstream ss;

    if (papdu->start != START) {
        log.pushMsg("ERROR: no start in frame");
        return;
    }

    // TODO: ca
    //    if (papdu->head.ca != slaveAddr && papdu->length > 6) {
    //        log.pushMsg("ERROR: parse ASDU with unexpected origin");
    //        return;
    //    }

    if (size == 6) {  // U格式APDU
        switch (papdu->NS) {
        case STARTDTCON: {
            log.pushMsg("INFO: U(STARTDTCON)");
            t1Timeout = -1;  // 接收到U启动帧确认报文后t1超时控制失效
        } break;
        case STOPDTACT: {  // 停止数据传送命令
            log.pushMsg("INFO: U(STOPDTACT)");
            if (isSend) {
                sendStopDtCon();
            }
        } break;
        case STOPDTCON:  // 停止数据传送命令确认
            // TODO:
            break;
        case TESTFRACT: {
            // 链路测试命令
            log.pushMsg("INFO: U(TESTFRACT)");
            if (isSend) {
                sendTestfrCon();
            }
        } break;
        case TESTFRCON:  // 链路测试命令确认
            break;
        case SUPERVISORY: {  // S帧
            log.pushMsg("INFO: S(SUPERVISORY)");
            if (vs >= papdu->NR) {  // NOTE: 考虑到NR可能比当前vs小
                numMsgUnack = vs - papdu->NR;
                if (numMsgUnack < CLIENTK) {
                    allowSend = true;
                }
            } else {
                ss.str("");
                ss << "ERROR: wrong N(R) of SUPERVISORY message="
                   << papdu->NR << ", while current vs is " << vs;
                log.pushMsg(ss.str().c_str());
                // TODO: Disconnect tcp when we get wrong sequence.
                return;
            }
        } break;
        default:
            log.pushMsg("ERROR: unknown control message");
            break;
        }
    } else {  // I format message

        // check vr
        vrReceived = papdu->NS >> 1;
        if (vrReceived != vr) {
            ss.str("");
            ss << "ERROR: N(R) is " << uint32_t(this->vr)
               << ", but the N(S) of I message is " << vrReceived;
            log.pushMsg(ss.str().c_str());
            if (ifCheckSeq) {
                ss.str("");
                ss << "WARNING: start to disconnect tcp";
                log.pushMsg(ss.str().c_str());
                tcpDisconnect();
                return;
            }
        } else {
            ++vr;
        }

        // check vs
        if (vs >= papdu->NR) {
            numMsgUnack = vs - papdu->NR;
            if (numMsgUnack < CLIENTK) {
                allowSend = true;
            }
        } else {
            ss.str("");
            ss << "ERROR: parse wrong N(R) of msg " << umapType2String[papdu->head.type]
               << ", the N(R) is " << papdu->NR << ", while current vs is "
               << vs;
            log.pushMsg(ss.str().c_str());
            // TODO: disconnect
            return;
        }

        ss.str("");
        if (isSend) {
            ss << "INFO: I (";
        }
        ss << umapType2String[papdu->head.type] << ")\n";
        ss << "    CA: " << uint32_t(papdu->head.ca) << "; TYP: " << uint32_t(papdu->head.type)
           << "; COT: " << int(papdu->head.cot)
           << "; SQ: " << uint32_t(papdu->head.sq)
           << "; NUM: " << uint32_t(papdu->head.num);
        ss << "    N(S): " << papdu->NS << "; N(R): " << papdu->NR << "\n";
        log.pushMsg(ss.str().c_str());

        switch (papdu->head.type) {
        case M_SP_NA_1: {  // 1: single-point information
            struct iec_type1* pobj;
            // TODO:

        } break;
        case M_SP_TA_1:  // 2: single-point information with time
            // tag(cp24time2a)
            break;
        case M_BO_NA_1: {  // 7: bitstring of 32 bits
            // 监视方向过程信息的应用服务数据单元
            struct iec_type7* pobj;
            struct iec_obj* piecarr = new iec_obj[papdu->head.num];
            uint32_t addr24 = 0;  // 24位信息对象地址

            for (int i = 0; i < papdu->head.num; ++i) {
                if (papdu->head.sq) {
                    pobj = &papdu->sq7.obj[i];
                    if (i == 0) {
                        addr24 = papdu->sq7.ioa16 +
                                ((uint32_t)papdu->sq7.ioa8 << 16);
                    } else {
                        ++addr24;
                    }
                } else {
                    pobj = &papdu->nsq7[i].obj;
                    addr24 = papdu->nsq7[i].ioa16 +
                            ((uint32_t)papdu->nsq7[i].ioa8 << 16);
                }

                piecarr[i].address = addr24;
                piecarr[i].type = papdu->head.type;
                piecarr[i].ca = papdu->head.ca;
                piecarr[i].cause = papdu->head.cot;
                piecarr[i].pn = papdu->head.pn;
                piecarr[i].bsi = pobj->bsi;
                piecarr[i].value = (float)pobj->bsi;
                piecarr[i].ov = pobj->ov;
                piecarr[i].bl = pobj->bl;
                piecarr[i].sb = pobj->sb;
                piecarr[i].nt = pobj->nt;
                piecarr[i].iv = pobj->iv;
            }

            dataIndication(piecarr, papdu->head.num);
            delete[] piecarr;
        } break;
        case M_BO_TA_1:  // 8: bitstring of 32 bits with time
            // tag(cp24time2a)
            break;
        case C_SC_NA_1:  // 45: single command
            break;
        case C_BO_NA_1:  // 51: bitstring of 32 bit command
            break;
        case M_EI_NA_1:  // 70:end of initialization(初始化结束)
            break;

        case C_IC_NA_1:  // 100: general interrogation
            // 主站收到总召唤确认报文 或 总召唤激活结束报文
            // TODO:

            break;
        default:
            break;
        }

        // TODO: 修改策略
        // timeout control settings
        if (t2Timeout < 0) {
            t2Timeout = T2;
        } else if (t2Timeout == 0) {
            t2Timeout = -1;
            sendMonitorMessage();
        } else {  // t2Timeout > 0
            --t2Timeout;
        }
        // update t3
        t3Timeout = T3;

        // check parameter `k` and `w`
        if (isSend) {
            ++numMsgReceived;
            if (numMsgReceived == CLIENTW) {
                sendMonitorMessage();
                numMsgReceived = 0;
            }
        }
    }
}
