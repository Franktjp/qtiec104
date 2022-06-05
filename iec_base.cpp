#include "iec_base.h"
#include <time.h>
#include <sstream>

using namespace std;

iec_base::iec_base() {
    this->slavePort = SERVERPORT;  // set iec104 tcp port to 2404
    qstrncpy(this->slaveIP, "", 20);
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
    qstrncpy(this->slaveIP, ip, 20);
}

uint16_t iec_base::getSlaveAddr() {
    return this->slaveAddr;
}

void iec_base::setSlaveAddr(uint16_t addr) {
    this->slaveAddr = addr;
}

/**
 * @brief iec_base::packetReadyTCP - tcp packets are ready to be read from the
 * connection with slave station.
 * TODO: 这是抄的,重写吧
 */
void iec_base::packetReadyTCP() {
    static bool brokenMsg = false;
    static apdu a;
    uint8_t* br;
    br = (uint8_t*)&a;
    int32_t bytesrec;  // TODO: readTCP返回结果，表示获取到的字节数
    uint8_t byt, len;  // 长度
    char buf[1000];

    while (true) {
        if (!brokenMsg) {
            // 找到START
            do {
                bytesrec = readTCP((char*)br, 1);
                if (bytesrec == 0) {
                    return;  // 该函数被调用说明这里不应该被执行到
                }
                byt = br[0];
            } while (byt != START);

            // 找到len
            bytesrec = readTCP((char*)br + 1, 1);
            if (bytesrec == 0) {
                return;
            }
        }

        len = br[1];
        if (len < 4) {  // 小于4说明是错误的帧，重新寻找
            brokenMsg = false;
            log.pushMsg("--> ERROR: invalid frame");
            continue;
        }

        // 读取除了68和length剩下的部分
        waitForReadyRead(len, 300);  // 等待len字节的数据被准备好
        bytesrec = readTCP((char*)br + 2, len);
        if (bytesrec == 0) {
            log.pushMsg("--> broken msg");
            brokenMsg = true;
            return;
        } else if (bytesrec < len) {  // 读取字节数小于len，重新读一次
            int rest = len - bytesrec;
            sprintf(buf, "--> should reread %d(%d in total)", rest, len);
            log.pushMsg(buf);

            // 第二次读
            waitForReadyRead(rest, 300);
            bytesrec = readTCP((char*)br + 2 + bytesrec, rest);
            sprintf(buf, "--> reread %d bytes in fact", bytesrec);
            if (bytesrec < rest) {
                log.pushMsg("--> broken msg");
                brokenMsg = true;
                return;
            }
        }

        // TODO: 可能需要验证ca

        brokenMsg = false;
        if (log.isLogging()) {
            int total = 25;  // 总共输出total字节
            sprintf(buf, "--> %03d: ", int(len + 2));
            for (int i = 0; i < len + 2 && i < total; ++i) {
                sprintf(buf + strlen(buf), "%02x ", br[i]);
            }
            log.pushMsg(buf);
        }
        parse(&a, a.length + 2);
        break;
    }
}

void iec_base::showFrame(const char* buf, int size, bool isSend) {
    char buffer[200];

    if (log.isLogging()) {
        memset(buffer, 0, sizeof(buffer));
        if (isSend) {
            sprintf(buffer, "send --> size: (%03d) ", size);
        } else {
            sprintf(buffer, "receive --> size: (%03d) ", size);
        }
        int cnt = 20, i;
        for (i = 0; i < size && i < cnt; ++i) {
            sprintf(buffer + strlen(buffer), "%02x ", buf[i]);
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
    // TODO: 先测试U帧启动，稍后取消注释
    // TODO:
    //    sendStartDtAct();  // 请求建立通信链路(主站->从站)
    log.pushMsg("connect success!");
}

void iec_base::onTcpDisconnect() {
    this->isConnected = false;
    log.pushMsg("disconnect success!");
    // TODO: 超时控制
    this->t1Timeout = -1;
    this->t2Timeout = -1;
    qDebug() << "disconnect success";
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
    log.pushMsg("send STARTDTACT");

    t1Timeout = T1;  // reset timeout t1
}

void iec_base::sendStopDtAct() {
    struct apdu wapdu;
    wapdu.start = START;
    wapdu.length = 4;
    wapdu.NS = uint16_t(STOPDTACT);
    wapdu.NR = 0;
    send(wapdu);
    log.pushMsg("send STOPDTACT");
}

void iec_base::sendStopDtCon() {
    struct apdu wapdu;
    wapdu.start = START;
    wapdu.length = 4;
    wapdu.NS = uint16_t(STOPDTCON);
    wapdu.NR = 0;
    send(wapdu);
    log.pushMsg("send STOPDTCON");
}

void iec_base::sendTestfrAct() {
    struct apdu wapdu;
    wapdu.start = START;
    wapdu.length = 4;
    wapdu.NS = uint16_t(TESTFRACT);
    wapdu.NR = 0;
    send(wapdu);
    log.pushMsg("send TESTFRACT");
}

void iec_base::sendTestfrCon() {
    struct apdu wapdu;
    wapdu.start = START;
    wapdu.length = 4;
    wapdu.NS = uint16_t(TESTFRCON);
    wapdu.NR = 0;
    send(wapdu);
    log.pushMsg("send TESTFRCON");
}

void iec_base::sendMonitorMessage() {
    struct apdu wapdu;
    wapdu.start = START;
    wapdu.length = 4;
    wapdu.NS = SUPERVISORY;
    wapdu.NR = vr << 1;
    send(wapdu);
    char buf[100];
    sprintf(buf, "send monitor frame(S), the N(R) is %d", wapdu.NR);
    log.pushMsg(buf);
}

void iec_base::generalInterrogationAct() {
    struct apdu a;
    a.start = START;
    a.length = 0x0E;
    a.NS = vs << 1;
    a.NR = vr << 1;
    a.head.type = INTERROGATION;  // TODO: 为何是INTERROGATION
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

    send(a);
    ++vs;
}

/**
 * @brief iec_base::sendCommand - Send command to slave station.
 * @return
 */
bool iec_base::sendCommand(struct iec_obj* obj) {
    struct apdu cmd;
    stringstream oss;
    time_t t = time(nullptr);
    struct tm* timeinfo = localtime(&t);

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
                oss << "SINGLE COMMAND\n    ADDRESS: " << (uint32_t)obj->address
                    << ", TYPE: " << (uint32_t)obj->type
                    << ", SCS: " << (uint32_t)obj->scs
                    << ", QU: " << (uint32_t)obj->qu
                    << ", SE: " << (uint32_t)obj->se
                    << ", COT: " << (uint32_t)obj->cause;
                log.pushMsg(oss.str().c_str());
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
                oss << "DOUBLE COMMAND\n    ADDRESS" << (uint32_t)obj->address
                    << ", TYPE: " << (uint32_t)obj->type
                    << ", DCS: " << (uint32_t)obj->dcs
                    << ", QU: " << (uint32_t)obj->qu
                    << ", SE: " << (uint32_t)obj->se
                    << ", COT: " << (uint32_t)obj->cause;
                log.pushMsg(oss.str().c_str());
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
                oss << "STEP ADJUST COMMAND\n    ADDRESS"
                    << (uint32_t)obj->address
                    << ", TYPE: " << (uint32_t)obj->type
                    << ", RCS: " << (uint32_t)obj->rcs
                    << ", QU: " << (uint32_t)obj->qu
                    << ", SE: " << (uint32_t)obj->se
                    << ", COT: " << (uint32_t)obj->cause;
                log.pushMsg(oss.str().c_str());
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
                oss << "SET COMMAND(NORMALIZED VALUE)\n    ADDRESS"
                    << (uint32_t)obj->address
                    << ", TYPE: " << (uint32_t)obj->type
                    << ", RCS: " << (uint32_t)obj->rcs
                    << ", QU: " << (uint32_t)obj->qu
                    << ", SE: " << (uint32_t)obj->se
                    << ", COT: " << (uint32_t)obj->cause;
                log.pushMsg(oss.str().c_str());
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
                oss << "SINGLE COMMAND WITH CP56TIME2A\n    ADDRESS"
                    << (uint32_t)obj->address
                    << ", TYPE: " << (uint32_t)obj->type
                    << ", SCS: " << (uint32_t)obj->scs
                    << ", QU: " << (uint32_t)obj->qu
                    << ", SE: " << (uint32_t)obj->se
                    << ", COT: " << (uint32_t)obj->cause;
                oss << "\n    TIME.YEAR" << cmd.nsq58.obj.time.year  // 年 2022
                    << ", TIME.MONTH" << cmd.nsq58.obj.time.mon      // 月 5
                    << ", TIME.DAY" << cmd.nsq58.obj.time.dmon       // 日 24
                    << ", TIME.HOUR" << cmd.nsq58.obj.time.hour      // 时 14
                    << ", TIME.MIN" << cmd.nsq58.obj.time.min        // 分 53
                    << ", TIME.SRC" << timeinfo->tm_sec;             // 秒 21
                log.pushMsg(oss.str().c_str());
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
                oss << "CLOCK SYNCHRONIZATION\n    ADDRESS"
                    << (uint32_t)obj->address
                    << ", TYPE: " << (uint32_t)obj->type
                    << ", COT: " << (uint32_t)cmd.head.cot;

                oss << "\n    TIME.YEAR" << cmd.nsq103.obj.time.year  // 年 2022
                    << ", TIME.MONTH" << cmd.nsq103.obj.time.mon      // 月 5
                    << ", TIME.DAY" << cmd.nsq103.obj.time.dmon       // 日 24
                    << ", TIME.HOUR" << cmd.nsq103.obj.time.hour      // 时 14
                    << ", TIME.MIN" << cmd.nsq103.obj.time.min        // 分 53
                    << ", TIME.SRC" << timeinfo->tm_sec;              // 秒 21
                log.pushMsg(oss.str().c_str());
            }
        } break;

        default:
            return false;
            break;
    }

    return true;
}

/**
 * @brief iec_base::parse
 * @param papdu
 * @param size
 * @param isSend - If we want to send the result to server.
 */
void iec_base::parse(struct apdu* papdu, int size, bool isSend) {
    struct apdu wapdu;  // 缓冲区组装发送apdu
    uint16_t vr_new;    // TODO
    stringstream ss;

    if (papdu->start != START) {
        log.pushMsg("ERROR: no start in frame");
        return;
    }

    // TODO:
    //    if (papdu->head.ca != slaveAddr && papdu->length > 6) {
    //        log.pushMsg("ERROR: parse ASDU with unexpected origin");
    //        return;
    //    }

    if (size == 6) {  // U格式APDU长度为6（2 + 4）
        switch (papdu->NS) {
            case STARTDTCON: {
                log.pushMsg("receive STARTDTCON");
                t1Timeout = -1;  // 接收到U启动帧确认报文后t1超时控制失效
            } break;
            case STOPDTACT: {  // 停止数据传送命令
                log.pushMsg("receive STOPDTACT");
                if (isSend) {
                    sendStopDtCon();
                }
            } break;
            case STOPDTCON:  // 停止数据传送命令确认
                // TODO:
                break;
            case TESTFRACT: {
                // 链路测试命令
                log.pushMsg("   TESTFRACT");
                if (isSend) {
                    sendTestfrCon();
                }
            } break;
            case TESTFRCON:  // 链路测试命令确认
                break;
            default:
                log.pushMsg("ERROR: unknown control message");
                break;
        }
    } else {    // I format message
        // TODO: 超时控制
        // code...

        // check vr
        vr_new = papdu->NS >> 1;
        if (vr_new != vr) {
            ss.str("");
            ss << "ERROR: N(R) is " << uint32_t(this->vr) << ", but the N(S) of I message is " << vr_new;
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

        ss.str("");
        ss << "     COT: " << papdu->head.ca << "; TYP: " << papdu->head.type
           << "; COT: " << int(papdu->head.cot)
           << "; SQ: " << (unsigned int)(papdu->head.sq)
           << "; NUM: " << papdu->head.num;
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
                //            // case INTERROGATION:
                //            // 从站收到类型标识100的报文后：
                //            // 总召唤确认
                //            generalInterrogationCon();
                //            // 发送遥测与遥信数据
                //            // TODO:

                //            // 总召唤结束
                //            generalInterrogationEnd();
                //            break;
            default:
                break;
        }

        // TODO: 修改策略
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
    }
}
