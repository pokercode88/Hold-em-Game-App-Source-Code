#include "UserStateProcessor.h"
#include "globe.h"
#include "LogComm.h"
#include "DataProxyProto.h"
#include "ServiceDefine.h"
#include "util/tc_hash_fun.h"
#include "uuid.h"
#include "PushServer.h"

//
using namespace std;
using namespace dataproxy;
using namespace dbagent;

UserStateProcessor::UserStateProcessor()
{

}

UserStateProcessor::~UserStateProcessor()
{

}

//查询
int UserStateProcessor::selectUserStateOnline(long uid, userstate::UserOnlineState &onlineState)
{
    dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(USER_STATE_ONLINE) + ":" + L2S(uid);
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    dataReq.clusterInfo.frageFactor = uid;
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.queryType = E_SELECT;

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "gwaddr";
    tfield.colType = STRING;
    fields.push_back(tfield);
    tfield.colName = "gwcid";
    tfield.colType = STRING;
    fields.push_back(tfield);
    dataReq.fields = fields;

    TReadDataRsp dataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(uid)->redisRead(dataReq, dataRsp);
    ROLLLOG_DEBUG << "online state, iRet: " << iRet<< ", uid: "<< uid << ", dataRsp: " << printTars(dataRsp) << endl;
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "online state err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }

    long gwCid = 0;
    string gwAddr = "";
    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        for (auto itaddr = it->begin(); itaddr != it->end(); ++itaddr)
        {
            if (itaddr->colName == "gwaddr")
            {
                gwAddr = itaddr->colValue;
            }
            else if (itaddr->colName == "gwcid")
            {
                gwCid = S2L(itaddr->colValue);
            }
        }
    }

    onlineState.state  = userstate::E_ONLINE_STATE_ONLINE;
    onlineState.gwAddr = gwAddr;
    onlineState.gwCid  = gwCid;
    return 0;
}

//增加
int UserStateProcessor::UpdateUserStateOnline(long uid, const string &gwAddr, const long gwCid)
{
    dataproxy::TWriteDataReq wdataReq;
    wdataReq.resetDefautlt();
    wdataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(USER_STATE_ONLINE) + ":" + L2S(uid);
    wdataReq.operateType = E_REDIS_WRITE;
    wdataReq.clusterInfo.resetDefautlt();
    wdataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    wdataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    wdataReq.clusterInfo.frageFactor = uid;

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "gwaddr";
    tfield.colType = STRING;
    tfield.colValue = gwAddr;
    fields.push_back(tfield);
    tfield.colName = "gwcid";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(gwCid);
    fields.push_back(tfield);
    wdataReq.fields = fields;

    TWriteDataRsp wdataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(uid)->redisWrite(wdataReq, wdataRsp);
    if (iRet != 0 || wdataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "save user access addr data err, iRet: " << iRet << ", iResult: " << wdataRsp.iResult << endl;
        return -1;
    }

    //ROLLLOG_DEBUG << "set user access addr data, iRet: " << iRet << ", wdataRsp: " << printTars(wdataRsp) << endl;
    return 0;
}

//删除
int UserStateProcessor::deleteUserStateOnline(long uid)
{
    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(USER_STATE_ONLINE) + ":" + L2S(uid);
    dataReq.operateType = E_REDIS_DELETE;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    dataReq.clusterInfo.frageFactor = uid;

    TWriteDataRsp dataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(uid)->redisWrite(dataReq, dataRsp);
    ROLLLOG_DEBUG << "delete user access addr data, iRet: " << iRet << ", datareq: " << printTars(dataReq) << ", dataRsp: " << printTars(dataRsp) << endl;
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "delete user access addr err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }

    return 0;
}

//查询
int UserStateProcessor::selectUserStateGame(long uid, userstate::GameState &gameState)
{
    dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(USER_STATE_GAME) + ":" + L2S(uid);
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    dataReq.clusterInfo.frageFactor = uid;
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.queryType = E_SELECT;

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = dbagent::E_NONE;
    tfield.colName = "game_addr";
    tfield.colType = STRING;
    fields.push_back(tfield);
    tfield.colName = "room_id";
    tfield.colType = STRING;
    fields.push_back(tfield);
    tfield.colName = "table_id";
    tfield.colType = STRING;
    fields.push_back(tfield);
    tfield.colName = "match_id";
    tfield.colType = STRING;
    fields.push_back(tfield);
    tfield.colName = "enter_room_time";
    tfield.colType = STRING;
    fields.push_back(tfield);
    tfield.colName = "gold";
    tfield.colType = BIGINT;
    fields.push_back(tfield);
    dataReq.fields = fields;

    TReadDataRsp dataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(uid)->redisRead(dataReq, dataRsp);
    ROLLLOG_DEBUG << "game state, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "game state err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }

    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        for (auto itaddr = it->begin(); itaddr != it->end(); ++itaddr)
        {
            if (itaddr->colName == "game_addr")
            {
                gameState.gameAddr = itaddr->colValue;
            }
            else if (itaddr->colName == "room_id")
            {
                gameState.sRoomID = itaddr->colValue;
            }
            else if (itaddr->colName == "table_id")
            {
                gameState.tableID = S2I(itaddr->colValue);
            }
            else if (itaddr->colName == "match_id")
            {
                gameState.matchID = itaddr->colValue;
            }
            else if (itaddr->colName == "gold")
            {
                gameState.gold = S2L(itaddr->colValue);
            }
            else if (itaddr->colName == "enter_room_time")
            {
                gameState.enterRoomTime = g_app.getOuterFactoryPtr()->GetTimeTick(itaddr->colValue);
            }
        }
    }

    return 0;
}

//增加
int UserStateProcessor::UpdateGameInfo(long uid, const string &gameAddr, const string &sRoomID)
{
    dataproxy::TWriteDataReq wdataReq;
    wdataReq.resetDefautlt();
    wdataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(USER_STATE_GAME) + ":" + L2S(uid);
    wdataReq.operateType = E_REDIS_WRITE;
    wdataReq.clusterInfo.resetDefautlt();
    wdataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    wdataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    wdataReq.clusterInfo.frageFactor = uid;

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    fields.clear();
    tfield.colName = "game_addr";
    tfield.colType = STRING;
    tfield.colValue = gameAddr;
    fields.push_back(tfield);
    tfield.colName = "room_id";
    tfield.colType = STRING;
    tfield.colValue = sRoomID;
    fields.push_back(tfield);
    tfield.colName = "enter_room_time";
    tfield.colType = STRING;
    tfield.colValue = g_app.getOuterFactoryPtr()->GetTimeFormat();
    fields.push_back(tfield);
    wdataReq.fields = fields;

    TWriteDataRsp wdataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(uid)->redisWrite(wdataReq, wdataRsp);
    ROLLLOG_DEBUG << "set user game addr data, iRet: " << iRet << ", wdataRsp: " << printTars(wdataRsp) << endl;
    if (iRet != 0 || wdataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "save user game addr data err, iRet: " << iRet << ", iResult: " << wdataRsp.iResult << endl;
        return -1;
    }

    return 0;
}

//删除
int UserStateProcessor::deleteGameInfo(long uid)
{
    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(USER_STATE_GAME) + ":" + L2S(uid);
    dataReq.operateType = E_REDIS_DELETE;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    dataReq.clusterInfo.frageFactor = uid;

    TWriteDataRsp dataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(uid)->redisWrite(dataReq, dataRsp);
    ROLLLOG_DEBUG << "delete user game addr data, iRet: " << iRet << ", datareq: " << printTars(dataReq) << ", dataRsp: " << printTars(dataRsp) << endl;
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "delete user game addr err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }

    return 0;
}

//重置
int UserStateProcessor::resetGameInfo(long uid)
{
    dataproxy::TWriteDataReq wdataReq;
    wdataReq.resetDefautlt();
    wdataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(USER_STATE_GAME) + ":" + L2S(uid);
    wdataReq.operateType = E_REDIS_WRITE;
    wdataReq.clusterInfo.resetDefautlt();
    wdataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    wdataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    wdataReq.clusterInfo.frageFactor = uid;
    wdataReq.paraExt.resetDefautlt();
    wdataReq.paraExt.queryType = E_REPLACE;

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    fields.clear();
    tfield.colName = "game_addr";
    tfield.colType = STRING;
    tfield.colValue = "";
    fields.push_back(tfield);
    tfield.colName = "room_id";
    tfield.colType = STRING;
    tfield.colValue = "";
    fields.push_back(tfield);
    tfield.colName = "table_id";
    tfield.colType = STRING;
    tfield.colValue = "0";
    fields.push_back(tfield);
    tfield.colName = "gold";
    tfield.colType = BIGINT;
    tfield.colValue = "0";
    fields.push_back(tfield);
    tfield.colName = "match_id";
    tfield.colType = INT;
    tfield.colValue = "0";
    fields.push_back(tfield);
    tfield.colName = "enter_room_time";
    tfield.colType = STRING;
    tfield.colValue = "1970-01-01 08:00:00";
    fields.push_back(tfield);
    wdataReq.fields = fields;

    TWriteDataRsp wdataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(uid)->redisWrite(wdataReq, wdataRsp);
    ROLLLOG_DEBUG << "reset user game info, iRet: " << iRet << ", wdataRsp: " << printTars(wdataRsp) << endl;
    if (iRet != 0 || wdataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "reset user game info err, iRet: " << iRet << ", iResult: " << wdataRsp.iResult << endl;
        return -1;
    }

    return 0;
}

//重置
int UserStateProcessor::resetGameInfoDB(long uid)
{
    dataproxy::TWriteDataReq wdataReq;
    wdataReq.resetDefautlt();
    wdataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(USER_STATE_GAME_DB) + ":" + L2S(uid);
    wdataReq.operateType = E_REDIS_WRITE;
    wdataReq.clusterInfo.resetDefautlt();
    wdataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    wdataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    wdataReq.clusterInfo.frageFactor = uid;
    wdataReq.paraExt.resetDefautlt();
    wdataReq.paraExt.queryType = E_REPLACE;

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    fields.clear();
    tfield.colName = "room_id";
    tfield.colType = STRING;
    tfield.colValue = "";
    fields.push_back(tfield);
    tfield.colName = "blind_id";
    tfield.colType = STRING;
    tfield.colValue = "0";
    fields.push_back(tfield);
    tfield.colName = "status";
    tfield.colType = BIGINT;
    tfield.colValue = "0";
    fields.push_back(tfield);
    tfield.colName = "game_type";
    tfield.colType = INT;
    tfield.colValue = "0";
    fields.push_back(tfield);
    wdataReq.fields = fields;

    TWriteDataRsp wdataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(uid)->redisWrite(wdataReq, wdataRsp);
    ROLLLOG_DEBUG << "reset user game info, iRet: " << iRet << ", wdataRsp: " << printTars(wdataRsp) << endl;
    if (iRet != 0 || wdataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "reset user game info err, iRet: " << iRet << ", iResult: " << wdataRsp.iResult << endl;
        return -1;
    }

    return 0;
}

//查询
int UserStateProcessor::CountStatistics(userstate::OnlinePlayerListResp &resp, bool isExcludeRobot)
{
    auto robots = g_app.getOuterFactoryPtr()->getRobotList();
    wbl::ReadLocker lock(m_rwlock);
    for (auto iter = m_mapOnline.begin(); iter != m_mapOnline.end(); iter++)
    {
        auto uid = (*iter).first;
        if (uid <= 0)
        {
            continue;
        }

        if (isExcludeRobot)
        {
            auto iter2 = std::find(robots.begin(), robots.end(), uid);
            if (iter2 != robots.end())
                continue;
        }

        resp.uidList.push_back(uid);
    }

    return resp.uidList.size();
}

//增加
int UserStateProcessor::UpdateStatistics(long uid)
{
    updateOnlineUser(uid, 0L, "");
    return 0;
}

//删除
int UserStateProcessor::deleteStatistics()
{
    wbl::WriteLocker lock(m_rwlock);
    m_mapOnline.clear();
    return 0;
}

//移除
int UserStateProcessor::rmStatistics(long uid)
{
    if (uid <= 0)
    {
        return 0;
    }

    wbl::WriteLocker lock(m_rwlock);
    auto iter = m_mapOnline.find(uid);
    if (iter != m_mapOnline.end())
    {
        ROLLLOG_DEBUG << "@delete user onlineinfo, uid: " << (*iter).first << endl;
        m_mapOnline.erase(iter);
    }

    return 0;
}

//获取在线用户标识
int UserStateProcessor::getOnlineUsers(std::map<long, long> &uids, bool isExcludeRobot)
{
    wbl::ReadLocker lock(m_rwlock);

    ROLLLOG_DEBUG << "----------------------------------------------" << endl;

    auto robots = g_app.getOuterFactoryPtr()->getRobotList();
    for (auto iter = m_mapOnline.begin(); iter != m_mapOnline.end(); iter++)
    {
        auto uid = (*iter).first;
        if (uid <= 0)
        {
            continue;
        }

        auto addr = (*iter).second;
        if (isExcludeRobot)
        {
            auto iter2 = std::find(robots.begin(), robots.end(), uid);
            if (iter2 != robots.end())
                continue;
        }

        ROLLLOG_DEBUG << "@online user, uid: " << uid << ", addr: " << addr << endl;
        uids.insert(std::pair<long, long>(uid, (*iter).second));
    }

    ROLLLOG_DEBUG << "----------------------------------------------" << endl;
    return 0;
}

//获取在线用户标识(随机取n个 优先玩家)
int UserStateProcessor::getOnlineUsersNew(std::vector<long> &uids, int LimitNum)
{
    wbl::ReadLocker lock(m_rwlock);

    ROLLLOG_DEBUG << "----------------------------------------------" << endl;

    std::vector<long> onlint_user;
    std::vector<long> onlint_robot;

    auto robots = g_app.getOuterFactoryPtr()->getRobotList();
    for (auto iter = m_mapOnline.begin(); iter != m_mapOnline.end(); iter++)
    {
        auto uid = (*iter).first;
        if (uid <= 0)
        {
            continue;
        }

        auto iter2 = std::find(robots.begin(), robots.end(), uid);
        if(iter2 != robots.end())
        {
            onlint_robot.push_back(uid);
        }
        else
        {
            onlint_user.push_back(uid);
        }
        ROLLLOG_DEBUG << "@online user, uid: " << uid << endl;
    }

    if((int)onlint_user.size() >= LimitNum)
    {
        std::random_shuffle(onlint_user.begin(), onlint_user.end());
        uids.insert(uids.begin(), onlint_user.begin(), onlint_user.begin() + LimitNum - 1);
    }
    else
    {
        uids.insert(uids.begin(), onlint_user.begin(), onlint_user.end());
        /* if((int)uids.size() < LimitNum)
         {
             if((int)onlint_robot.size() > (LimitNum - (int)uids.size()))
             {
                 int exNum = onlint_robot.size() - (LimitNum - (int)uids.size());
                 uids.insert(uids.begin(), onlint_robot.begin(), onlint_robot.begin() + exNum - 1);
             }
             else
             {
                 uids.insert(uids.begin(), onlint_robot.begin(), onlint_robot.end());
             }
         }*/
    }
    ROLLLOG_DEBUG << "----------------------------------------------" << endl;
    return 0;
}

//更新在线用户信息
int UserStateProcessor::updateOnlineUser(const long uid, const long cid, const std::string &addr)
{
    if (uid <= 0)
    {
        return 0;
    }

    long nowTime = TNOW;
    wbl::WriteLocker lock(m_rwlock);
    auto iter = m_mapOnline.find(uid);
    if (iter != m_mapOnline.end())
    {
        (*iter).second = nowTime;
        ROLLLOG_DEBUG << "@update user(1), uid: " << uid << ", nowTime: " << nowTime << ", updateTime: " << (*iter).second << endl;
    }
    else
    {
        m_mapOnline.insert(std::pair<long, long>(uid, nowTime));
        ROLLLOG_DEBUG << "@update user(2), uid: " << uid << ", nowTime: " << nowTime << endl;
    }


    return 0;
}

//清除在线信息冗余
int UserStateProcessor::cleanOnlineUser(bool isExcludeRobot)
{
    std::vector<long> v;

    //检查冗余在线信息
    {
        const long nowTime = TNOW;
        const long interval = 150;
        wbl::ReadLocker lock(m_rwlock);
        for (auto iter = m_mapOnline.begin(); iter != m_mapOnline.end(); iter++)
        {
            long uid = (*iter).first;
            long lastUpdateTime = (*iter).second;
            if (lastUpdateTime == 0)
                continue;

            if ((uid <= 0) || (nowTime >= lastUpdateTime + interval))
                v.push_back(uid);
        }
    }

    //清除冗余在线信息
    {
        auto robots = g_app.getOuterFactoryPtr()->getRobotList();
        wbl::WriteLocker lock(m_rwlock);
        for (auto iter1 = v.begin(); iter1 != v.end(); iter1++)
        {
            auto uid = *iter1;

            //机器人排除在外
            if (isExcludeRobot)
            {
                auto iter2 = std::find(robots.begin(), robots.end(), uid);
                if (iter2 != robots.end())
                    continue;
            }

            //清除冗余信息
            auto iter3 = m_mapOnline.find(uid);
            if (iter3 != m_mapOnline.end())
            {
                ROLLLOG_DEBUG << "@clean user onlineinfo, uid: " << uid << endl;
                m_mapOnline.erase(iter3);
            }
        }

        v.clear();
    }

    return 0;
}
