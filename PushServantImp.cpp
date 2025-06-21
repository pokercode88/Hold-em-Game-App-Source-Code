#include "PushServantImp.h"
#include "servant/Application.h"
#include "LogComm.h"
#include "globe.h"
#include "JFGameHttpProto.h"
#include "CommonStruct.h"
#include "CommonCode.h"
#include "XGameComm.pb.h"
#include "CommonCode.pb.h"
#include "CommonStruct.pb.h"
#include "push.pb.h"
#include "DataProxyProto.h"
#include "ServiceDefine.h"
#include "util/tc_hash_fun.h"
#include "PushServer.h"
#include "Push.h"
#include "push.pb.h"
#include "PushProto.h"
#include "UserState.pb.h"
#include "UserStateProcessor.h"

#include "Java2RoomProto.h"
#include "RoomServant.h"
using namespace JFGame;

//
using namespace std;
using namespace JFGame;
using namespace JFGameHttpProto;
using namespace push;
using namespace userstate;
using namespace DaqiGame;
using namespace dataproxy;

//推送服务通知
std::atomic<bool> g_PushUpdateNotify(false);
//剩余更新时间
long g_RemainingTime = 0;


//房间列表共享锁
static wbl::ReadWriteLocker m_lockRoom_seat_info;
//房间桌位信息
static map<string, push::RoomTableInfo> _room_seat_info;

//拆分字符串
static vector<std::string> split(const string &str, const string &pattern)
{
    return TC_Common::sepstr<string>(str, pattern);
}

//////////////////////////////////////////////////////
void PushServantImp::initialize()
{
    //initialize servant here:
    //...
}

//////////////////////////////////////////////////////
void PushServantImp::destroy()
{
    _room_seat_info.clear();
    //destroy servant here:
    //...
}

//http请求处理接口
tars::Int32 PushServantImp::doRequest(const vector<tars::Char> &reqBuf, const map<std::string, std::string> &extraInfo, vector<tars::Char> &rspBuf, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");

    int iRet = 0;

    __TRY__

    ROLLLOG_DEBUG << "rec doRequest, reqBuf size : " << reqBuf.size() << endl;

    if (reqBuf.empty())
    {
        iRet = -1;
        return iRet;
    }

    THttpPackage thttpPack;
    THttpPackage thttpPackRsp;

    __TRY__
    if (!reqBuf.empty())
    {
        toObj(reqBuf, thttpPack);
    }
    __CATCH__

    if (thttpPack.vecData.empty())
    {
        iRet = -2;
        return iRet;
    }

    CommonReqHead reqHead;
    CommonRespHead rspHead;

    __TRY__
    if (!thttpPack.vecData.empty())
    {
        toObj(thttpPack.vecData, reqHead);
    }
    __CATCH__

    ROLLLOG_DEBUG << "request head, actionName id: " << reqHead.actionName << ", actionName: " << etos((ActionName)reqHead.actionName) << endl;

    if (reqHead.reqBodyBytes.empty())
    {
        iRet = -3;
        return iRet;
    }

    switch (reqHead.actionName)
    {
    default:
    {
        ROLLLOG_ERROR << "invalid msgid." << endl;
        iRet = -101;
        return iRet;
    }
    }

    thttpPackRsp.stUid.lUid = thttpPack.stUid.lUid;
    thttpPackRsp.stUid.sToken = thttpPack.stUid.sToken;
    thttpPackRsp.iVer = thttpPack.iVer;
    thttpPackRsp.iSeq = thttpPack.iSeq;
    tobuffer(rspHead, thttpPackRsp.vecData);
    tobuffer(thttpPackRsp, rspBuf);

    ROLLLOG_DEBUG << "response buff size: " << rspBuf.size() << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

//tcp请求处理接口
tars::Int32 PushServantImp::onRequest(tars::Int64 lUin, const std::string &sMsgPack, const std::string &sCurServrantAddr, const JFGame::TClientParam &stClientParam, const JFGame::UserBaseInfoExt &stUserBaseInfo, tars::TarsCurrentPtr current)
{
    int iRet = 0;

    __TRY__

    ROLLLOG_DEBUG << "recv msg, uid : " << lUin << ", addr : " << stClientParam.sAddr << endl;

    PushServant::async_response_onRequest(current, 0);

    XGameComm::TPackage pkg;
    pbToObj(sMsgPack, pkg);
    if (pkg.vecmsghead_size() == 0)
    {
        ROLLLOG_ERROR << "package empty." << endl;
        return -1;
    }

    ROLLLOG_DEBUG << "recv msg, uid : " << lUin << ", msg : " << logPb(pkg) << endl;

    for (int i = 0; i < pkg.vecmsghead_size(); ++i)
    {
        int64_t ms1 = TNOWMS;

        auto &head = pkg.vecmsghead(i);
        switch (head.nmsgid())
        {
        case XGameProto::ActionName::USER_STATE_BATCH_ONLINE_STATE: // = 401;// 批量取在线状态
        {
            UserStateProto::BatchOnlineStateReq batchOnlineStateReq;
            pbToObj(pkg.vecmsgdata(i), batchOnlineStateReq);
            iRet = onBatchOnlineState(pkg, batchOnlineStateReq, sCurServrantAddr);
            break;
        }
        case XGameProto::ActionName::USER_STATE_GET_GAME_STATE: // = 400;// 获取用户游戏状态
        {
            UserStateProto::GetGameStateReq gameStateReq;
            pbToObj(pkg.vecmsgdata(i), gameStateReq);
            iRet = onGetGameState(pkg, gameStateReq, sCurServrantAddr);
            break;
        }
        case XGameProto::ActionName::USER_STATE_BATCH_GAME_STATE: // = 403;// 批量取用户游戏状态
        {
            UserStateProto::BatchGameStateReq batchGameStateReq;
            pbToObj(pkg.vecmsgdata(i), batchGameStateReq);
            iRet = onBatchGameState(pkg, batchGameStateReq, sCurServrantAddr);
            break;
        }
        case XGameProto::ActionName::USER_STATE_COUNT_STATISTICS: // = 402;// 统计在线数量
        {
            iRet = onCountStatistics(pkg, sCurServrantAddr);
            break;
        }
        default:
        {
            ROLLLOG_ERROR << "invalid msg id, uid: " << lUin << ", msg id: " << head.nmsgid() << endl;
            break;
        }
        }

        if (iRet != 0)
        {
            ROLLLOG_ERROR << "process err, uid: " << lUin << ", iRet: " << iRet << endl;
        }

        int64_t ms2 = TNOWMS;
        if ((ms2 - ms1) > COST_MS)
        {
            ROLLLOG_WARN << "@Performance, msgid:" << head.nmsgid() << ", costTime:" << (ms2 - ms1) << endl;
        }
    }

    __CATCH__

    return iRet;
}

//推送消息
tars::Int32 PushServantImp::pushMsg(const push::PushMsgReq &req, tars::TarsCurrentPtr current)
{
    int iRet = 0;

    __TRY__

    if (req.msg.size() == 0)
    {
        ROLLLOG_ERROR << "parameter empty, size: " << req.msg.size() << endl;
        return -1;
    }
    userstate::BatchOnlineStateReq batchOnlineStateReq;
    for (auto it = req.msg.begin(); it != req.msg.end(); ++it)
    {
        batchOnlineStateReq.uidList.push_back(it->uid);
    }

    userstate::BatchOnlineStateResp batchOnlineStateResp;
    iRet = batchOnlineState(batchOnlineStateReq, batchOnlineStateResp, current);
    if (iRet != 0)
    {
        ROLLLOG_ERROR << "get user online state err, iRet: " << iRet << endl;
        return -2;
    }

    //LOG_DEBUG << "batchOnlineStateResp: "<< printTars(batchOnlineStateResp)<< endl;
    for (auto it = req.msg.begin(); it != req.msg.end(); ++it)
    {
        string accessAddr = it->accessAddr;
        if (accessAddr.empty())
        {
            auto itonline = batchOnlineStateResp.data.find(it->uid);
            if (itonline != batchOnlineStateResp.data.end())
            {
                accessAddr = itonline->second.gwAddr;
            }
        }

        if (accessAddr.empty())
        {
            continue;
        }

        switch (it->msgType)
        {
        case E_PUSH_MSG_TYPE_MATCH_BEGIN: //比赛开始消息
        {
            push::MatchBeginNotify notify;
            notify = toObj<push::MatchBeginNotify>(it->vecData);

            PushProto::MatchBeginNotify matchBeginNotify;
            matchBeginNotify.set_matchid(notify.matchID);
            matchBeginNotify.set_sroomid(notify.sRoomID);
            XGameComm::TPackage pkg;
            auto ptuid = pkg.mutable_stuid();
            ptuid->set_luid(it->uid);
            toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_MATCH_BEGIN, matchBeginNotify);
            break;
        }
        case E_PUSH_MSG_TYPE_MULTIPLE_LOGIN: //重复登录消息
        {
            PushProto::DefaultNotify notify;
            notify.set_resultcode(0);

            XGameComm::TPackage pkg;
            auto ptuid = pkg.mutable_stuid();
            ptuid->set_luid(it->uid);
            toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_MULTIPLE_LOGIN, notify);
            break;
        }
        case E_PUSH_MSG_TYPE_COIN_CHANGE: //财富改变消息
        {
            push::CoinNotify coinNotify;
            coinNotify = toObj<push::CoinNotify>(it->vecData);

            PushProto::CoinNotify notify;
            notify.set_changetype(it->changeType);
            notify.set_gold(coinNotify.gold);
            notify.set_diamond(coinNotify.diamond);
            notify.set_club_gold(coinNotify.club_gold);
            notify.set_point(coinNotify.point);
            notify.set_level(coinNotify.level);
            notify.set_experience(coinNotify.experience);
            notify.set_vip_level(coinNotify.vipLevel);
            notify.set_vip_expired(coinNotify.vipExpired);

            ROLLLOG_DEBUG << "push msg, req: " << logPb(notify) << ", uid: " << it->uid << endl;

            XGameComm::TPackage pkg;
            auto ptuid = pkg.mutable_stuid();
            ptuid->set_luid(it->uid);
            toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_COIN_CHANGE, notify);
            break;
        }
        case E_PUSH_MSG_TYPE_CHAT_UPDATE: // = 3,    //聊天消息变更推送
        {
            PushProto::DefaultNotify notify;
            notify.set_resultcode(0);

            XGameComm::TPackage pkg;
            auto ptuid = pkg.mutable_stuid();
            ptuid->set_luid(it->uid);
            toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_CHAT_UPDATE, notify);
            break;
        }
        case E_PUSH_MSG_TYPE_INVITE_PLAYER: // = 5,  //邀请玩家
        {
            push::InvitedPlayerNotify notify;
            notify = toObj<push::InvitedPlayerNotify>(it->vecData);

            PushProto::InvitedPlayerNotify invitedPlayerNotify;
            invitedPlayerNotify.set_sroomid(notify.sRoomID);
            invitedPlayerNotify.set_sroomname(notify.sRoomName);
            invitedPlayerNotify.set_snickname(notify.sNickName);
            invitedPlayerNotify.set_lplayerid(notify.lPlayerID);
            invitedPlayerNotify.set_lbigblind(notify.lBigBlind);
            invitedPlayerNotify.set_lsmallblind(notify.lSmallBlind);
            invitedPlayerNotify.set_matchid(notify.matchID);
            invitedPlayerNotify.set_tableid(notify.tableID);
            invitedPlayerNotify.set_sroomkey(notify.sRoomKey);
            XGameComm::TPackage pkg;
            auto ptuid = pkg.mutable_stuid();
            ptuid->set_luid(it->uid);
            toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_INVITE_PLAYER, invitedPlayerNotify);
            break;
        }
        case E_PUSH_MSG_TYPE_ADD_FRIEND: // = 6,  //添加好友
        {
            push::AddFriendNotify notify;
            notify = toObj<push::AddFriendNotify>(it->vecData);

            PushProto::AddFriendNotify addFriendNotify;
            addFriendNotify.set_uid(notify.uid);
            addFriendNotify.set_name(notify.name);
            addFriendNotify.set_head(notify.head);
            addFriendNotify.set_gender(notify.gender);
            addFriendNotify.set_logout_time(notify.logout_time);
            addFriendNotify.set_type(notify.type);
            XGameComm::TPackage pkg;
            auto ptuid = pkg.mutable_stuid();
            ptuid->set_luid(it->uid);
            toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_ADD_FRIEND, addFriendNotify);
            break;
        }
        case E_PUSH_MSG_TYPE_TABLE_STAT_INFO: // = 7,  //房间信息变更
        {
            push::TableStatInfo notify;
            notify = toObj<push::TableStatInfo>(it->vecData);

            PushProto::TableStatInfo tableStatInfo;
            tableStatInfo.set_iclubid(notify.iClubId);
            tableStatInfo.set_sroomkey(notify.sRoomKey);
            tableStatInfo.set_iplayercount(notify.iPlayerCount);
            tableStatInfo.set_istatus(notify.iStatus);
            tableStatInfo.set_istarttime(notify.iStartTime);
            tableStatInfo.set_icosttime(notify.iCostTime);

            //ROLLLOG_DEBUG << "push msg, tableStatInfo: " << logPb(tableStatInfo) << ", uid: " << it->uid << endl;

            XGameComm::TPackage pkg;
            auto ptuid = pkg.mutable_stuid();
            ptuid->set_luid(it->uid);
            toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_MSG_TABLE_STAT_NOTIFY, tableStatInfo);
            break;
        }
        case E_PUSH_MSG_TYPE_GIVE_PROPS: // = 8,  //赠送道具
        {
            PushProto::DefaultNotify notify;
            notify.set_resultcode(0);

            XGameComm::TPackage pkg;
            auto ptuid = pkg.mutable_stuid();
            ptuid->set_luid(it->uid);
            toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_GIVE_PROPS, notify);
            break;
        }
        case E_PUSH_MSG_TYPE_RECHARGE_REWARDS: // = 9,  //充值奖励
        {
            break;
        }
        case E_PUSH_MSG_TYPE_NEW_MAIL_NOTIFY: // = 10,  //新邮件通知
        {
            break;
        }
        case E_PUSH_MSG_TYPE_SERVER_UPDATE: // = 11,  //服务器维护
        {
            push::ServerUpdateNotify notify;
            notify = toObj<push::ServerUpdateNotify>(it->vecData);

            PushProto::ServerUpdateNotify serverUpdateNotify;
            serverUpdateNotify.set_iminutes(notify.iMinutes);

            XGameComm::TPackage pkg;
            auto ptuid = pkg.mutable_stuid();
            ptuid->set_luid(it->uid);
            toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_SERVER_UPDATE_NOTIFY, serverUpdateNotify);
            break;
        }
        case E_PUSH_MSG_TYPE_BROADCAST_NOTIFY: // = 14,  //广播公告
        {
            push::BroadcastNotify tarsNotify;
            tarsNotify = toObj<push::BroadcastNotify>(it->vecData);

            PushProto::BroadcastNotify broadcastNotify;
            broadcastNotify.set_content(tarsNotify.content);
            broadcastNotify.set_id(tarsNotify.id);
            broadcastNotify.set_type(tarsNotify.type);
            auto &pbParam = *broadcastNotify.mutable_param();
            pbParam.set_interval(tarsNotify.param.interval);
            pbParam.set_count(tarsNotify.param.count);
            pbParam.set_begintime(tarsNotify.param.beginTime);
            pbParam.set_endtime(tarsNotify.param.endTime);
            
            XGameComm::TPackage pkg;
            auto ptuid = pkg.mutable_stuid();
            ptuid->set_luid(it->uid);
            toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_BROADCAST_NOTIFY, broadcastNotify);
            ROLLLOG_DEBUG << "broadcast massge: " << logPb(broadcastNotify) << ", uid: " << it->uid << endl;
            break;
        }
        case E_PUSH_MSG_TYPE_PROPS_CHANGE:
        {
            push::PropsChangeNotify notify;
            notify = toObj<push::PropsChangeNotify>(it->vecData);

            PushProto::PropsChangeNotify propsChangeNotify;
            propsChangeNotify.set_resultcode(0);
            propsChangeNotify.set_propsid(notify.iPropsID);
            propsChangeNotify.set_currcount(notify.lCurrCount);

            XGameComm::TPackage pkg;
            auto ptuid = pkg.mutable_stuid();
            ptuid->set_luid(it->uid);
            toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_PROPS_CHANGE_NOTIFY, propsChangeNotify);
            break;
        }
        case E_PUSH_MSG_TYPE_GET_BOX_NOTIFY://获得宝箱通知
        {
            break;
        }
        case E_PUSH_MSG_TYPE_CLUB_MAIL_NOTIFY://俱乐部审核邮件通知
        {
            break;
        }
        case E_PUSH_MSG_TYPE_MSG_REDDOT_NOTIFY:   // 大厅红点
        case E_PUSH_MSG_TYPE_CLUB_REDDOT_NOTIFY:  // 俱乐部申请红点
        case E_PUSH_MSG_TYPE_FRIEND_NOTIFY:       // 好友申请红点
        case E_PUSH_MSG_TYPE_UNION_REDDOT_NOTIFY: // 联盟申请红点
        {
            int iType = 0;
            if (it->msgType == E_PUSH_MSG_TYPE_CLUB_REDDOT_NOTIFY)
            {
                iType = 2;
            }
            else if (it->msgType == E_PUSH_MSG_TYPE_FRIEND_NOTIFY)
            {
                iType = 4;
            }
            else if (it->msgType == E_PUSH_MSG_TYPE_UNION_REDDOT_NOTIFY)
            {
                iType = 5;
            }
            
            PushProto::RedDotNotify redDotNotify;
            redDotNotify.set_itype(iType);
            XGameComm::TPackage pkg;
            auto ptuid = pkg.mutable_stuid();
            ptuid->set_luid(it->uid);

            ROLLLOG_DEBUG << "msgType: " << it->msgType << ", uid: " << it->uid << endl;
            toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_MSG_REDDOT_NOTIFY, redDotNotify);
            break;
        }
        case E_PUSH_MSG_TYPE_BLIND_TG_NOTIFY:
        {
            push::TGBindCallBackNotify notify;
            notify = toObj<push::TGBindCallBackNotify>(it->vecData);

            PushProto::TGBindCallBackNotify tGBindCallBackNotify;
            tGBindCallBackNotify.set_tgid(notify.tgId);
            tGBindCallBackNotify.set_tgname(notify.tgName);
            XGameComm::TPackage pkg;
            auto ptuid = pkg.mutable_stuid();
            ptuid->set_luid(it->uid);
            ROLLLOG_DEBUG << "bind tg notify: " << pbToString(tGBindCallBackNotify) << ", uid: " << it->uid << endl;
            toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_MSG_BIND_TG_NOTIFY, tGBindCallBackNotify);
            break;
        }
        default: //异常转发消息
        {
            ROLLLOG_ERROR << "invalid transmit msg:" << it->msgType << endl;
            break;
        }
        }
    }

    __CATCH__
    return iRet;
}

tars::Int32 PushServantImp::pushBroadcast(const push::BroadcastNotify &notify, tars::TarsCurrentPtr current)
{
    int iRet = 0;
    __TRY__
    
    userstate::OnlinePlayerListResp onlinePlayerListResp;
    int iRet = onlinePlayerList(onlinePlayerListResp, current);
    if (iRet != 0)
    {
        LOG_ERROR << "onlinePlayerList empty ret:" << iRet << endl;
        return -1;
    }

    push::PushMsgReq pushMsgReq;
    for (auto ituid = onlinePlayerListResp.uidList.begin(); ituid != onlinePlayerListResp.uidList.end(); ++ituid)
    {
        if (*ituid < 0)
        {
            ROLLLOG_ERROR << "user_id : " << *ituid << " error, notify failed!" << endl;
            continue;
        }

        push::PushMsg pushMsg;
        pushMsg.uid = *ituid;
        pushMsg.msgType = push::E_PUSH_MSG_TYPE_BROADCAST_NOTIFY;
        tobuffer(notify, pushMsg.vecData);
        pushMsgReq.msg.push_back(pushMsg);
    }

    if (!pushMsgReq.msg.empty())
    {
        pushMsg(pushMsgReq, current);
        LOG_DEBUG << "async_pushMsg, request._content:" << logTars(notify) << endl;
    }
    __CATCH__
    return iRet;    
}

//金币变化日志
int PushServantImp::PushUserInfo(const long uid, const string& accessAddr)
{
   /* userinfo::GetUserBasicReq basicReq;
    basicReq.uid = uid;
    userinfo::GetUserBasicResp basicResp;
    int iRet = g_app.getOuterFactoryPtr()->getHallServantPrx(uid)->getUserBasic(basicReq, basicResp);
    if (iRet != 0)
    {
        ROLLLOG_ERROR << " getUserBasic err. iRet:  " << iRet  << endl;
        return iRet;
    }

    PushProto::CoinNotify notify;
    notify.set_changetype(0);
    notify.set_gold(basicResp.userinfo.gold);
    notify.set_diamond(basicResp.userinfo.diamond);
    notify.set_club_gold(basicResp.userinfo.club_gold);
    notify.set_point(basicResp.userinfo.point);
    notify.set_level(basicResp.userinfo.level);
    notify.set_experience(basicResp.userinfo.experience);
    notify.set_vip_level(basicResp.userinfo.vip_level);
    notify.set_vip_expired(basicResp.userinfo.vip_expired);

    ROLLLOG_DEBUG << "push msg, req: " << logPb(notify) << ", uid: " << uid << endl;

    XGameComm::TPackage pkg;
    auto ptuid = pkg.mutable_stuid();
    ptuid->set_luid(uid);
    toClientPb(pkg, accessAddr, XGameProto::ActionName::PUSH_COIN_CHANGE, notify);*/

    return 0 ;
}

//报告在线状态
tars::Int32 PushServantImp::reportOnlineState(const userstate::ReportOnlineStateReq &req, userstate::ReportOnlineStateResp &resp, tars::TarsCurrentPtr current)
{
    int iRet = 0;
    __TRY__

    if (req.uid <= 0 || req.state < E_ONLINE_STATE_OFFLINE || req.state > E_ONLINE_STATE_ONLINE)
    {
        ROLLLOG_ERROR << "parameter err, uid: " << req.uid << ", state: " << req.state << endl;
        return -1;
    }

    //在线状态
    if (req.state == E_ONLINE_STATE_ONLINE)
    {
        if ((req.gwAddr != "") && (req.gwCid != 0))
        {
            PushProto::DefaultNotify notify;
            notify.set_resultcode(0);
            XGameComm::TPackage pkg;
            pkg.mutable_stuid()->set_luid(req.uid);
            toClientPb2(pkg, req.gwCid, E_ONLINE_STATE_ONLINE, req.gwAddr, XGameProto::ActionName::PUSH_MULTIPLE_LOGIN, notify);
            ROLLLOG_DEBUG << "@OnlineUser notify, uid: " << req.uid << ", gwAddr: " << req.gwAddr << ", gwCid: " << req.gwCid << endl;
        }

        //更新用户状态
        iRet = UserStateSingleton::getInstance()->UpdateUserStateOnline(req.uid, req.gwAddr, req.gwCid);
        if (iRet != 0)
        {
            ROLLLOG_ERROR << "save user access addr data err, iRet: " << iRet << endl;
            return -3;
        }

        //更新在线统计
        iRet = UserStateSingleton::getInstance()->UpdateStatistics(req.uid);
        if (iRet != 0)
        {
            ROLLLOG_ERROR << "update statistics err, iRet: " << iRet << ", uid: " << req.uid << endl;
            return -4;
        }

        //推送用户信息
        PushUserInfo(req.uid, req.gwAddr);

        ROLLLOG_DEBUG << "@OnlineUser curData, uid: " << req.uid << ", gwAddr: " << req.gwAddr << ", gwCid: " << req.gwCid << endl;
    }
    //离线状态
    else if (req.state == E_ONLINE_STATE_OFFLINE)
    {
        //删除用户状态
        int iRet = UserStateSingleton::getInstance()->deleteUserStateOnline(req.uid);
        if (iRet != 0)
        {
            ROLLLOG_ERROR << "delete user access addr err, iRet: " << iRet << endl;
            return -5;
        }

        //移除在线统计
        iRet = UserStateSingleton::getInstance()->rmStatistics(req.uid);
        if (iRet != 0)
        {
            ROLLLOG_ERROR << "remove statistics err, iRet: " << iRet << ", uid: " << req.uid << endl;
            return -6;
        }

        ROLLLOG_DEBUG << "@OfflineUser curData, uid: " << req.uid << ", gwAddr: " << req.gwAddr << ", gwCid: " << req.gwCid << endl;
    }

    __CATCH__
    return iRet;
}

//报告游戏状态
tars::Int32 PushServantImp::reportGameState(const userstate::ReportGameStateReq &req, userstate::ReportGameStateResp &resp, tars::TarsCurrentPtr current)
{
    int iRet = 0;
    __TRY__

    if (req.uid < 0 || req.state < E_GAME_STATE_OFF || req.state > E_GAME_STATE_ON)
    {
        ROLLLOG_ERROR << "parameter err, uid: " << req.uid << ", state: " << req.state << ", gameAddr: " << req.gameAddr.length() << endl;
        return -1;
    }

    iRet = GameState2db(req);
    iRet = GameState2Cache(req, current);

    //机器人加入在线列表
    UserStateSingleton::getInstance()->updateOnlineUser(req.uid, 0L, "");

    __CATCH__

    return iRet;
}

int PushServantImp::GameState2db(const userstate::ReportGameStateReq &req)
{
    int iRet = 0;
    //在线状态
    if (req.state == E_GAME_STATE_ON)
    {
        dataproxy::TWriteDataReq wdataReq;
        wdataReq.resetDefautlt();
        wdataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(USER_STATE_GAME_DB) + ":" + L2S(req.uid);
        wdataReq.operateType = E_REDIS_WRITE;
        wdataReq.clusterInfo.resetDefautlt();
        wdataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
        wdataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
        wdataReq.clusterInfo.frageFactor = req.uid;
        wdataReq.paraExt.resetDefautlt();
        wdataReq.paraExt.queryType = E_REPLACE;

        vector<TField> fields;
        TField tfield;
        tfield.colArithType = E_NONE;
        fields.clear();
        tfield.colName = "room_id";
        tfield.colType = STRING;
        tfield.colValue = req.sRoomID;
        fields.push_back(tfield);
        tfield.colName = "blind_id";
        tfield.colType = STRING;
        tfield.colValue = I2S(req.blindID);
        fields.push_back(tfield);
        tfield.colName = "game_type";
        tfield.colType = STRING;
        tfield.colValue = I2S(req.gameType);
        fields.push_back(tfield);
        wdataReq.fields = fields;

        TWriteDataRsp wdataRsp;
        iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(req.uid)->redisWrite(wdataReq, wdataRsp);
        ROLLLOG_DEBUG << "set user game addr data, iRet: " << iRet
                      << ", wDataReq: " << printTars(wdataReq)
                      << ", wDataRsp: " << printTars(wdataRsp) << endl;
        if (iRet != 0 || wdataRsp.iResult != 0)
        {
            ROLLLOG_ERROR << "save user game addr data err, iRet: " << iRet << ", iResult: " << wdataRsp.iResult << endl;
            return -2;
        }
    }
    //离线状态
    else if (req.state == E_GAME_STATE_OFF)
    {
        iRet = UserStateSingleton::getInstance()->resetGameInfoDB(req.uid);
        if (iRet != 0)
        {
            ROLLLOG_ERROR << "resetGameInfo err, iRet: " << iRet << ", req.uid: " << req.uid << endl;
            return -3;
        }
    }
    return iRet;
}


int PushServantImp::GameState2Cache(const userstate::ReportGameStateReq &req, tars::TarsCurrentPtr current)
{
    int iRet = 0;
    //在线状态
    if (req.state == E_GAME_STATE_ON)
    {
        dataproxy::TWriteDataReq wdataReq;
        wdataReq.resetDefautlt();
        wdataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(USER_STATE_GAME) + ":" + L2S(req.uid);
        wdataReq.operateType = E_REDIS_WRITE;
        wdataReq.clusterInfo.resetDefautlt();
        wdataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
        wdataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
        wdataReq.clusterInfo.frageFactor = req.uid;
        wdataReq.paraExt.resetDefautlt();
        wdataReq.paraExt.queryType = E_REPLACE;

        vector<TField> fields;
        TField tfield;
        tfield.colArithType = E_NONE;
        fields.clear();
        tfield.colName = "game_addr";
        tfield.colType = STRING;
        tfield.colValue = req.gameAddr;
        fields.push_back(tfield);
        tfield.colName = "room_id";
        tfield.colType = STRING;
        tfield.colValue = req.sRoomID;
        fields.push_back(tfield);
        tfield.colName = "table_id";
        tfield.colType = STRING;
        tfield.colValue = I2S(req.tableID);
        fields.push_back(tfield);
        tfield.colName = "gold";
        tfield.colType = BIGINT;
        tfield.colValue = L2S(req.gold);
        fields.push_back(tfield);
        tfield.colName = "match_id";
        tfield.colType = INT;
        tfield.colValue = L2S(req.matchID);
        fields.push_back(tfield);
        tfield.colName = "enter_room_time";
        tfield.colType = STRING;
        tfield.colValue = g_app.getOuterFactoryPtr()->GetTimeFormat();
        fields.push_back(tfield);
        wdataReq.fields = fields;

        TWriteDataRsp wdataRsp;
        iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(req.uid)->redisWrite(wdataReq, wdataRsp);
        ROLLLOG_DEBUG << "set user game addr data, iRet: " << iRet
                      << ", wDataReq: " << printTars(wdataReq)
                      << ", wDataRsp: " << printTars(wdataRsp) << endl;
        if (iRet != 0 || wdataRsp.iResult != 0)
        {
            ROLLLOG_ERROR << "save user game addr data err, iRet: " << iRet << ", iResult: " << wdataRsp.iResult << endl;
            return -2;
        }
    }
    //离线状态
    else if (req.state == E_GAME_STATE_OFF)
    {
        userstate::GameStateReq gameStateReq;
        gameStateReq.uid = req.uid;
        userstate::GameStateResp gameStateResp;
        int iRet = gameState(gameStateReq, gameStateResp, current);
        if ((iRet != 0) || (gameStateResp.data.enterRoomTime <= 0) || (gameStateResp.data.enterRoomTime > TNOW))
        {
            ROLLLOG_ERROR << "get gameState failed, uid: " << req.uid << endl;
            return -1;
        }

        if (!gameStateResp.data.gameAddr.empty())
        {
            //更新用户当天累计玩牌时间(秒)
            updateUserCurIngameTime(req.uid, current);
            //重置用户游戏信息
            iRet = UserStateSingleton::getInstance()->resetGameInfo(req.uid);
            if (iRet != 0)
            {
                ROLLLOG_ERROR << "resetGameInfo err, iRet: " << iRet << ", req.uid: " << req.uid << endl;
                return -3;
            }
        }
    }
    return iRet;
}

long PushServantImp::formatAccumulate(const std::string param_1, std::string param_2, std::string param_3)
{
    long result = 0;
    std::string param = param_1 + "|" + param_2;
    if (!param_1.empty() || !param_2.empty())
    {
        vector<std::string> vParams = split(param, "|");
        for(auto item : vParams)
        {
            vector<std::string> vSubParams = split(item, ":");
            if (vSubParams.size() != 2)
            {
                continue;
            }
            result += S2L(vSubParams[1]);
        }
    }
    else
    {
        vector<std::string> vParams = split(param_3, "|");
        result = vParams.size();
    }
    return result;
}

tars::Int32 PushServantImp::reportRoomUsers(const std::string &roomId, const std::string &uids, const std::string &blindUids, const std::string &robotWin, long onlineCount, long winNum, tars::TarsCurrentPtr current)
{
    int iRet = 0;

    __TRY__


    __CATCH__
    return iRet;
}

//取玩家补偿金币
long PushServantImp::getUserCompensateGold(long uid)
{
    long iRet = 0;
    return iRet;
}

int PushServantImp::updateUserCurIngameTime(tars::Int64 uid, tars::TarsCurrentPtr current)
{
    return 0;
}

int PushServantImp::checkUserInNoviceDays(tars::Int64 uid, const int &subType, tars::TarsCurrentPtr current)
{
    return 0;
}

//通知所有在线玩家
int PushServantImp::serverUpdateNotify(int minutes)
{
    int iRet = 0;
    __TRY__

    if (minutes <= 0)
    {
        ROLLLOG_ERROR << "param error, minutes : " << minutes << endl;
        return -1;
    }

    userstate::OnlinePlayerListResp onlinePlayerListResp;
    int onLineNum = UserStateSingleton::getInstance()->CountStatistics(onlinePlayerListResp);
    if (onLineNum < 0)
    {
        ROLLLOG_ERROR << "CountStatistics failed, onLineNum : " << onLineNum << ", minutes : " << minutes << endl;
        iRet = -1;
    }
    else if (onLineNum == 0)
    {
        ROLLLOG_DEBUG << "CountStatistics failed, onLineNum : " << onLineNum << " no need notify, minutes : " << minutes << endl;
    }
    else
    {
        push::PushMsgReq pushMsgReq;
        for (auto ituid = onlinePlayerListResp.uidList.begin(); ituid != onlinePlayerListResp.uidList.end(); ++ ituid)
        {
            if (*ituid < 0)
            {
                ROLLLOG_ERROR << "CountStatistics user_id : " << *ituid << " error, notify failed!" << endl;
                continue;
            }

            push::PushMsg pushMsg;
            pushMsg.uid = *ituid;
            pushMsg.msgType = push::E_PUSH_MSG_TYPE_SERVER_UPDATE;

            push::ServerUpdateNotify serverUpdateNotify;
            serverUpdateNotify.iMinutes = minutes;
            tobuffer(serverUpdateNotify, pushMsg.vecData);
            pushMsgReq.msg.push_back(pushMsg);

            ROLLLOG_DEBUG << "server update notify ituid : " << *ituid << endl;
        }

        if ((int)pushMsgReq.msg.size() > 0)
        {
            pushMsg(pushMsgReq, NULL);
        }
    }

    __CATCH__
    return iRet;
}


//补偿玩家金币
tars::Int32 PushServantImp::returnCompensateGold(const std::string &roomId, tars::TarsCurrentPtr current)
{
    return 0;
}

//在线状态
tars::Int32 PushServantImp::onlineState(const userstate::OnlineStateReq &req, userstate::OnlineStateResp &resp, tars::TarsCurrentPtr current)
{
    int iRet = 0;
    __TRY__

    if (req.uid < 0)
    {
        ROLLLOG_ERROR << "parameter err, uid: " << req.uid << endl;
        return -1;
    }

    UserOnlineState uos;
    iRet = UserStateSingleton::getInstance()->selectUserStateOnline(req.uid, uos);
    if (iRet == 0 && !uos.gwAddr.empty())
    {
        resp.resultCode  = 0;
        resp.data.state  = E_ONLINE_STATE_ONLINE;
        resp.data.gwAddr = uos.gwAddr;
        resp.data.gwCid  = uos.gwCid;
    }
    else
    {
        resp.resultCode  = 0;
        resp.data.state  = E_ONLINE_STATE_OFFLINE;
        resp.data.gwAddr = "";
        resp.data.gwCid  = 0;
    }
    __CATCH__
    return iRet;
}

//批量取在线状态
tars::Int32 PushServantImp::batchOnlineState(const userstate::BatchOnlineStateReq &req, userstate::BatchOnlineStateResp &resp, tars::TarsCurrentPtr current)
{
    int iRet = 0;
    __TRY__

    if (req.uidList.empty())
    {
        ROLLLOG_ERROR << "parameter err, uid: " << req.uidList.size() << endl;
        return -1;
    }

    for (auto iter = req.uidList.begin(); iter != req.uidList.end(); iter++)
    {
        if ((*iter) < 0)
            continue;

        UserOnlineState userOnlineState;
        iRet = UserStateSingleton::getInstance()->selectUserStateOnline(*iter, userOnlineState);
        if (iRet == 0)
        {
            userOnlineState.state  = E_ONLINE_STATE_ONLINE;
            resp.data[*iter] = userOnlineState;
        }
        else
        {
            userOnlineState.state  = E_ONLINE_STATE_OFFLINE;
            resp.data[*iter] = userOnlineState;
        }
    }

    resp.resultCode = 0;

    __CATCH__
    return iRet;
}

//获取游戏状态
tars::Int32 PushServantImp::gameState(const userstate::GameStateReq &req, userstate::GameStateResp &resp, tars::TarsCurrentPtr current)
{
    int iRet = 0;
    __TRY__

    if (req.uid < 0)
    {
        ROLLLOG_ERROR << "parameter err, uid: " << req.uid << endl;
        return -1;
    }

    userstate::GameState gameState;
    iRet = UserStateSingleton::getInstance()->selectUserStateGame(req.uid, gameState);
    if (iRet != 0)
    {
        ROLLLOG_ERROR << "game state err, iRet: " << iRet << endl;
        return -2;
    }

    resp.resultCode         = 0;
    resp.data.state         = userstate::E_GAME_STATE_ON;
    resp.data.gameAddr      = gameState.gameAddr;
    resp.data.sRoomID       = gameState.sRoomID;
    resp.data.tableID       = gameState.tableID;
    resp.data.matchID       = gameState.matchID;
    resp.data.enterRoomTime = gameState.enterRoomTime;

    __CATCH__
    return iRet;
}

//批量获取游戏状态
tars::Int32 PushServantImp::batchGameState(const userstate::BatchGameStateReq &req, userstate::BatchGameStateResp &resp, tars::TarsCurrentPtr current)
{
    int iRet = 0;
    __TRY__

    if (req.uidList.empty())
    {
        ROLLLOG_ERROR << "parameter err, uid: " << req.uidList.size() << endl;
        return -1;
    }

    for (auto ituid = req.uidList.begin(); ituid != req.uidList.end(); ++ituid)
    {
        long uid = *ituid;
        if (uid <= 0)
            continue;

        GameState gs;
        int ret = UserStateSingleton::getInstance()->selectUserStateGame(uid, gs);
        if (ret != 0)
            continue;

        GameState gameState;
        gameState.state    = E_GAME_STATE_ON;
        gameState.gameAddr = gs.gameAddr;
        gameState.sRoomID  = gs.sRoomID;
        gameState.tableID  = gs.tableID;
        resp.data[*ituid]  = gameState;
    }

    resp.resultCode = 0;

    __CATCH__
    return iRet;
}

//在线玩家列表
tars::Int32 PushServantImp::onlinePlayerList(userstate::OnlinePlayerListResp &resp, tars::TarsCurrentPtr current)
{
    int iRet = 0;
    __TRY__

    std::map<long, long> users;
    iRet = UserStateSingleton::getInstance()->getOnlineUsers(users);
    if (iRet == 0)
    {
        for (auto iter = users.begin(); iter != users.end(); iter++)
        {
            resp.uidList.push_back((*iter).first);
        }
    }
    resp.resultCode = 0;

    __CATCH__
    return iRet;
}

//在线玩家列表
tars::Int32 PushServantImp::getlinePlayerList(int limitNum, userstate::OnlinePlayerListResp &resp, tars::TarsCurrentPtr current)
{
    int iRet = 0;
    __TRY__

    std::vector<long> users;
    iRet = UserStateSingleton::getInstance()->getOnlineUsersNew(users, limitNum);
    if (iRet == 0)
    {
        for (auto iter = users.begin(); iter != users.end(); iter++)
        {
            resp.uidList.push_back(*iter);
        }
    }
    resp.resultCode = 0;

    __CATCH__
    return iRet;
}

//获取游戏状态
int PushServantImp::onGetGameState(const XGameComm::TPackage &pkg, const UserStateProto::GetGameStateReq &gameStateReq, const std::string &sCurServrantAddr)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    UserStateProto::GetGameStateResp getGameStateResp;

    long lUid = gameStateReq.uid();
    if (lUid <= 0)
    {
        //兼容处理:如果请求协议里不带uid,则使用请求用户uid
        lUid = pkg.stuid().luid();
    }

    userstate::GameState gs;
    iRet = UserStateSingleton::getInstance()->selectUserStateGame(lUid, gs);
    if (iRet != 0)
    {
        ROLLLOG_ERROR << "game state err, iRet: " << iRet << endl;
        getGameStateResp.set_resultcode(XGameRetCode::SYS_BUSY);
        toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_STATE_GET_GAME_STATE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_USER_STATE, getGameStateResp);
        return -1;
    }

    getGameStateResp.set_resultcode(0);

    if ((gs.sRoomID.size() >= 4) && (gs.sRoomID[3] == '3'))
    {
        auto it = _room_seat_info.find(gs.sRoomID);
        if (it != _room_seat_info.end())
        {
            getGameStateResp.set_sroomid(gs.sRoomID);
            getGameStateResp.set_tableid(gs.tableID);
            getGameStateResp.set_matchid(gs.matchID);

            auto itt = it->second.tableInfo.find(gs.tableID);
            if (itt != it->second.tableInfo.end())
            {
                getGameStateResp.set_level(itt->second.level);
                getGameStateResp.set_smallblind(itt->second.smallBlind);
                getGameStateResp.set_bigblind(itt->second.bigBlind);
                getGameStateResp.set_mingold(itt->second.minGold);
                getGameStateResp.set_maxgold(itt->second.maxGold);
                getGameStateResp.set_maxseat(itt->second.maxSeat);
            }
        }
    }
    else
    {
        getGameStateResp.set_sroomid(gs.sRoomID);
        getGameStateResp.set_tableid(gs.tableID);
        getGameStateResp.set_matchid(gs.matchID);
    }

    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_STATE_GET_GAME_STATE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_USER_STATE, getGameStateResp);
    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

//批量取在线状态
int PushServantImp::onBatchOnlineState(const XGameComm::TPackage &pkg, const UserStateProto::BatchOnlineStateReq &batchOnlineStateReq, const std::string &sCurServrantAddr)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    UserStateProto::BatchOnlineStateResp batchOnlineStateResp;
    if (batchOnlineStateReq.uidlist_size() == 0)
    {
        ROLLLOG_ERROR << "parameter err, uid size: " << batchOnlineStateReq.uidlist_size() << endl;
        batchOnlineStateResp.set_resultcode(XGameRetCode::ARG_INVALIDATE_ERROR);
        toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_STATE_BATCH_ONLINE_STATE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_USER_STATE, batchOnlineStateResp);
        return -1;
    }

    for (int i = 0; i < batchOnlineStateReq.uidlist_size(); ++i)
    {
        long uid = batchOnlineStateReq.uidlist(i);
        if (uid <= 0)
            continue;

        userstate::UserOnlineState resp;
        iRet = UserStateSingleton::getInstance()->selectUserStateOnline(uid, resp);
        if (iRet == 0)
        {
            UserStateProto::UserOnlineState userOnlineState;
            userOnlineState.set_state(UserStateProto::E_ONLINE_STATE_ONLINE);
            userOnlineState.set_accessaddr(resp.gwAddr);
            (*batchOnlineStateResp.mutable_data())[uid] = userOnlineState;
        }
        else
        {
            UserStateProto::UserOnlineState userOnlineState;
            userOnlineState.set_state(UserStateProto::E_ONLINE_STATE_OFFLINE);
            userOnlineState.set_accessaddr("");
            (*batchOnlineStateResp.mutable_data())[uid] = userOnlineState;
        }
    }

    batchOnlineStateResp.set_resultcode(0);
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_STATE_BATCH_ONLINE_STATE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_USER_STATE, batchOnlineStateResp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

//批量获取游戏状态
int PushServantImp::onBatchGameState(const XGameComm::TPackage &pkg, const UserStateProto::BatchGameStateReq &batchGameStateReq, const std::string &sCurServrantAddr)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    UserStateProto::BatchGameStateResp batchGameStateResp;
    if (batchGameStateReq.uidlist_size() == 0)
    {
        ROLLLOG_ERROR << "parameter err, uids_size: " << batchGameStateReq.uidlist_size() << endl;
        batchGameStateResp.set_resultcode(XGameRetCode::ARG_INVALIDATE_ERROR);
        toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_STATE_BATCH_GAME_STATE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_USER_STATE, batchGameStateResp);
        return -1;
    }

    for (int i = 0; i < batchGameStateReq.uidlist_size(); ++i)
    {
        long uid = batchGameStateReq.uidlist(i);
        if (uid <= 0)
            continue;

        GameState gs;
        iRet = UserStateSingleton::getInstance()->selectUserStateGame(uid, gs);
        if (iRet != 0)
            continue;

        UserStateProto::UserGameState userGameState;
        if ((gs.sRoomID != "") && (gs.sRoomID[0] == '3'))
        {
            auto it = _room_seat_info.find(gs.sRoomID);
            if (it != _room_seat_info.end())
            {
                userGameState.set_sroomid(gs.sRoomID);
                userGameState.set_tableid(gs.tableID);

                auto itt = it->second.tableInfo.find(gs.tableID);
                if (itt != it->second.tableInfo.end())
                {
                    userGameState.set_level(itt->second.level);
                    userGameState.set_smallblind(itt->second.smallBlind);
                    userGameState.set_bigblind(itt->second.bigBlind);
                    userGameState.set_mingold(itt->second.minGold);
                    userGameState.set_maxgold(itt->second.maxGold);
                    userGameState.set_maxseat(itt->second.maxSeat);
                }
            }
        }
        else
        {
            userGameState.set_sroomid(gs.sRoomID);
            userGameState.set_tableid(gs.tableID);
            userGameState.set_matchid(gs.matchID);
        }

        (*batchGameStateResp.mutable_data())[uid] = userGameState;
        ROLLLOG_DEBUG << "onBatchGameState, uid: " << uid << ", roomid: " << gs.sRoomID << ", _room_seat_info size:" << _room_seat_info.size() << ", gamestate:" << logPb(userGameState) << endl;
    }

    //正常应答
    batchGameStateResp.set_resultcode(0);
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_STATE_BATCH_GAME_STATE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_USER_STATE, batchGameStateResp);
    ROLLLOG_DEBUG << "onBatchGameState resp:" << logPb(batchGameStateResp) << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

//获取在线数量
int PushServantImp::onCountStatistics(const XGameComm::TPackage &pkg, const std::string &sCurServrantAddr)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    UserStateProto::CountStatisticsResp countStatisticsResp;

    userstate::OnlinePlayerListResp onlinePlayerListResp;
    iRet = UserStateSingleton::getInstance()->CountStatistics(onlinePlayerListResp);
    if (iRet < 0)
    {
        ROLLLOG_ERROR << "count statistics err, iRet: " << iRet << ", uid: " << pkg.stuid().luid() << endl;
        countStatisticsResp.set_resultcode(XGameRetCode::SYS_BUSY);
        toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_STATE_COUNT_STATISTICS, XGameComm::SERVICE_TYPE::SERVICE_TYPE_USER_STATE, countStatisticsResp);
        return -1;
    }

    countStatisticsResp.set_resultcode(0);
    countStatisticsResp.set_lcount(iRet + g_app.getOuterFactoryPtr()->getOnline());
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_STATE_COUNT_STATISTICS, XGameComm::SERVICE_TYPE::SERVICE_TYPE_USER_STATE, countStatisticsResp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

tars::Int32 PushServantImp::reportRoomTableInfo(const push::RoomTableInfo &data, tars::TarsCurrentPtr current)
{
    return 0;
}

tars::Int32 PushServantImp::serverUpdateNotifyAll(const int time, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    //通知所有在线玩家
    g_RemainingTime = time;
    g_PushUpdateNotify = true;

    ROLLLOG_DEBUG << "begin server update notify all, g_RemainingTime : " << g_RemainingTime << endl;

    serverUpdateNotify(g_RemainingTime);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

//推送消息红点
tars::Int32 PushServantImp::pushMessageRed(const int iType, tars::TarsCurrentPtr current)
{
    __TRY__

    userstate::OnlinePlayerListResp onlinePlayerListResp;
    int iRet = onlinePlayerList(onlinePlayerListResp, current);
    if (iRet != 0)
    {
        LOG_ERROR << "onlinePlayerList empty ret:" << iRet << endl;
        return -1;
    }

    push::PushMsgReq pushMsgReq;
    for (auto ituid = onlinePlayerListResp.uidList.begin(); ituid != onlinePlayerListResp.uidList.end(); ++ituid)
    {
        if (*ituid < 0)
        {
            ROLLLOG_ERROR << "user_id : " << *ituid << " error, notify failed!" << endl;
            continue;
        }

        push::PushMsg pushMsg;
        pushMsg.uid = *ituid;
        pushMsg.msgType = (push::E_Push_Msg_Type)iType;
        pushMsgReq.msg.push_back(pushMsg);
    }

    if (!pushMsgReq.msg.empty())
    {
        pushMsg(pushMsgReq, current);
    }

    __CATCH__
    return 0;
}

tars::Int32 PushServantImp::reportUserForbidden(const long uid, const int iType, const int fromGame, tars::TarsCurrentPtr current)
{
    ROLLLOG_DEBUG << "iType: " << iType<< ", uid:"<< uid << ", fromGame:"<< fromGame << endl;
    userstate::GameState gameState;
    int iRet = UserStateSingleton::getInstance()->selectUserStateGame(uid, gameState);
    if (iRet != 0)
    {
        ROLLLOG_ERROR << "game state err, iRet: " << iRet << endl;
        return 0;
    }

    UserOnlineState userOnlineState;
    iRet = UserStateSingleton::getInstance()->selectUserStateOnline(uid, userOnlineState);
    if (iRet != 0 || userOnlineState.gwAddr.empty())
    {
        LOG_DEBUG << "user not online. uid:"<< uid << endl;
        return 0;
    }

    if(iType == 1 || fromGame > 0 || gameState.gameAddr.empty())//直接通知
    {
        PushProto::UserForbiddenNotify userForbiddenNotify;
        userForbiddenNotify.set_uid(uid);
        userForbiddenNotify.set_bforbidden(iType == 1);
        XGameComm::TPackage pkg;
        auto ptuid = pkg.mutable_stuid();
        ptuid->set_luid(uid);
        ROLLLOG_DEBUG << "user forbidden notify: " << logPb(userForbiddenNotify) << endl;
        toClientPb(pkg, userOnlineState.gwAddr, XGameProto::ActionName::PUSH_MSG_USER_FORBIDDEN, userForbiddenNotify);
    }
    return 0;
}

tars::Int32 PushServantImp::onlineInfoUserList(const int type, int &userCount, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    if (type == 1)
    {
        //获取真实玩家数量
        userCount = 0;
        userstate::OnlinePlayerListResp onlinePlayerListResp;
        int onLineNum = UserStateSingleton::getInstance()->CountStatistics(onlinePlayerListResp, true);
        if (onLineNum < 0)
        {
            ROLLLOG_ERROR << "CountStatistics failed, onLineNum : " << onLineNum << ", type : " << type << endl;
            iRet = -1;
        }
        else
        {
            iRet = 0;
            ROLLLOG_DEBUG << "CountStatistics onLineNum: " << onLineNum << ", type : " << type << endl;
            userCount = onLineNum;
        }
    }
    else
    {
        ROLLLOG_ERROR << "type : " << type << " error!" << endl;
        iRet = -1;
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

//网关在线用户上报
tars::Int32 PushServantImp::onlineUserReport(const push::OnlineUserReportReq &req, push::OnlineUserReportResp &resp, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    ROLLLOG_DEBUG << "@report user, router: " << req.sRouterAddr << ", count: " << req.users.size() << endl;
    for (auto iter = req.users.begin(); iter != req.users.end(); iter++)
    {
        long uid = (*iter).first;
        long cid = (*iter).second;
        UserStateSingleton::getInstance()->updateOnlineUser(uid, cid, req.sRouterAddr);
        ROLLLOG_DEBUG << "@report user, uid: " << uid << ", cid: " << cid << endl;
    }

    resp.iResultCode = 0;
    __CATCH__
    FUNC_EXIT("", iRet);
    return 0;
}

//
tars::Int32 PushServantImp::doCustomMessage(bool bExpectIdle)
{
   /* if (g_PushUpdateNotify)
    {
        g_PushUpdateNotify = false;
        serverUpdateNotify(g_RemainingTime);
        LOG_WARN << "--------Send update notification messages to online players-----------" << endl;
    }*/

    return 0;
}

//发送消息到客户端
template<typename T>
int PushServantImp::toClientPb(const XGameComm::TPackage &tPackage, const std::string &sCurServrantAddr, XGameProto::ActionName actionName, const T &t)
{
    XGameComm::TPackage rsp;
    rsp.set_iversion(tPackage.iversion());
    rsp.mutable_stuid()->set_luid(tPackage.stuid().luid());
    rsp.set_igameid(tPackage.igameid());
    rsp.set_sroomid(tPackage.sroomid());
    rsp.set_iroomserverid(tPackage.iroomserverid());
    rsp.set_isequence(tPackage.isequence());
    rsp.set_iflag(tPackage.iflag());

    auto mh = rsp.add_vecmsghead();
    mh->set_nmsgid(actionName);
    mh->set_nmsgtype(XGameComm::MSGTYPE::MSGTYPE_NOTIFY);
    mh->set_servicetype(XGameComm::SERVICE_TYPE::SERVICE_TYPE_PUSH);
    rsp.add_vecmsgdata(pbToString(t));

    auto pPushPrx = Application::getCommunicator()->stringToProxy<JFGame::PushPrx>(sCurServrantAddr);
    if (pPushPrx)
    {
        pPushPrx->tars_hash(tPackage.stuid().luid())->async_doPushBuf(NULL, tPackage.stuid().luid(), pbToString(rsp));
    }
    else
    {
        ROLLLOG_ERROR << "pPushPrx is null:  " << endl;
    }

    return 0;
}

//发送消息到客户端
template<typename T>
int PushServantImp::toClientPb2(const XGameComm::TPackage &tPkg, const long cid, const int state, const string &addr, XGameProto::ActionName action, const T &t)
{
    XGameComm::TPackage rsp;
    rsp.set_iversion(tPkg.iversion());
    rsp.mutable_stuid()->set_luid(tPkg.stuid().luid());
    rsp.set_igameid(tPkg.igameid());
    rsp.set_sroomid(tPkg.sroomid());
    rsp.set_iroomserverid(tPkg.iroomserverid());
    rsp.set_isequence(tPkg.isequence());
    rsp.set_iflag(tPkg.iflag());

    auto mh = rsp.add_vecmsghead();
    mh->set_nmsgid(action);
    mh->set_nmsgtype(XGameComm::MSGTYPE::MSGTYPE_NOTIFY);
    mh->set_servicetype(XGameComm::SERVICE_TYPE::SERVICE_TYPE_PUSH);
    rsp.add_vecmsgdata(pbToString(t));

    string sPkg = pbToString(rsp);
    int64_t uid = tPkg.stuid().luid();
    g_app.getOuterFactoryPtr()->asyncBroadcastRouterServer(uid, cid, addr, state, sPkg);
    return 0;
}

template<typename T>
int PushServantImp::toClientPb(const XGameComm::TPackage &tPackage, const std::string &sCurServrantAddr, XGameProto::ActionName actionName, int serviceType, const T &t)
{
    XGameComm::TPackage rsp;
    rsp.set_iversion(tPackage.iversion());
    rsp.mutable_stuid()->set_luid(tPackage.stuid().luid());
    rsp.set_igameid(tPackage.igameid());
    rsp.set_sroomid(tPackage.sroomid());
    rsp.set_iroomserverid(tPackage.iroomserverid());
    rsp.set_isequence(tPackage.isequence());
    rsp.set_iflag(tPackage.iflag());

    auto mh = rsp.add_vecmsghead();
    mh->set_nmsgid(actionName);

    switch (actionName)
    {
    case XGameProto::ActionName::USER_STATE_GET_GAME_STATE:
        mh->set_nmsgtype(XGameComm::MSGTYPE::MSGTYPE_RESPONSE);
        break;
    default:
        mh->set_nmsgtype(XGameComm::MSGTYPE::MSGTYPE_NOTIFY);
        break;
    }

    mh->set_servicetype((XGameComm::SERVICE_TYPE)serviceType);
    rsp.add_vecmsgdata(pbToString(t));

    auto pPushPrx = Application::getCommunicator()->stringToProxy<JFGame::PushPrx>(sCurServrantAddr);
    if (pPushPrx)
    {
        pPushPrx->tars_hash(tPackage.stuid().luid())->async_doPushBuf(NULL, tPackage.stuid().luid(), pbToString(rsp));
    }
    else
    {
        ROLLLOG_ERROR << "pPushPrx is null, " << endl;
    }

    return 0;
}
