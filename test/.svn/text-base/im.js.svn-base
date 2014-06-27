/**
 * im 1.0.0.0 - 蜀山聊天
 * 
 * Copyright (c) 2008 FNQ
 * 
 * 
 * $Date: 2009-10-30 $
 * $Rev: 01 $
 *
 --------------------------------------------------
 更新日志

 */

function im() {
    /*==========================================================================
	 *
	 * 每个对象属性
	 * 可以进行外部设置
	 *
	 *==========================================================================*/

	this.host = "192.168.0.7";

	this.port = "6666";

	this.swf_path = "socket.swf";

	this.debug = 0;

	this.history_length = 7;

	this.chat_length = 30;

	this.nickname = null;

	this.login_key = null;

	this.private_key = null;

	this.block_word = null;

	this.block_word_s = /<[^>]*>?|\'|\"|\\|#|\||(?:\w+:\/\/)?(?:[0-9a-zA-Z\-_]+\.){2,}[0-9a-zA-Z\-_]+/gi;

	this.block_word_s2 = /<[^>]*>?|\'|\"|\\|#|(?:\w+:\/\/)?(?:[0-9a-zA-Z\-_]+\.){2,}[0-9a-zA-Z\-_]+/gi;

	this.block_word_s3 = /<(?:script|i?frame|style|html|body|title|link|meta|object).*>?/gi;

	this.block_time = 1000;

	this.world_desc = "欢迎进入 <span class='highlight'>聊天中心</span>！<br />请严格遵守<span class='highlight'>《互联网站禁止传播淫秽、色情等不良信息自律规范》</span>的相关规定，<br />严禁发布反动、淫秽、色情等不良信息，对违反规定者我们将主动向公安机关举报。<br />希望大家自觉遵守相关法律法规，文明交流，祝大家聊天愉快！";

	this.private_desc = "欢迎进入 <span class='highlight'>聊天中心</span>！<br />这里显示您与其他玩家的私密谈话。";

	//this.delay_link = 30000;

	this.delay_link = Math.round(Math.random()*10 + 10)*1000;

	this.sending = false;

	this.right_menu = new Array("加为好友", "查看信息");

	this.heart_beat_time = 300000;

	this.send_handle = null;

	this.channels = {
		'C' : {
			name : "联盟",
			view_online:true,
			desc : "这个是联盟"
		},
		'R' : {
			name: "场景"
		},
		'M' : {
			name : "师徒",
			view_online : true,
			member : {
				'50yqn' : '管理员'
			}
		}
	};

	this.chat_action = {
		ti : {
			//私聊的部分
			my : "你踢了 $t 一脚，$t 气喘吁吁的站了起来",
			target : "$p 踢了你一脚，你气喘吁吁的站了起来",
			//世界的部分
			world : "$p 朝石头踢了一脚，结果踢歪了，重重地摔在地上，国足的脚法",
			//频道的部分
			channel : "$p 朝石头踢了一脚，结果踢歪了，重重地摔在地上，国足的脚法"
		},
		ha : {
			my : "你朝 $t 哈哈大笑",
			target : "$p 对着你哈哈大笑",
			world : "$p 哈哈大笑",
			channel : "$p 哈哈大笑"
		},
		"@@" : {
			my : "你的一双眼睛瞪得比铜铃还大！越来越大……瞪破了！",
			target : "$p 瞪着一双比铜铃还大的眼睛向你挤眉弄眼。",
			world : "$p 的一双眼睛瞪得比铜铃还大！越来越大……瞪破了！",
			channel : "$p 的一双眼睛瞪得比铜铃还大！越来越大……瞪破了！"
		}
	};

	this.operation_icon = "icon/icon-operation.gif";

	this.error_msg = {
		too_fast : "说太快了，喝口水先~",
		user_not_exist : "该用户不在线或者不存在",
		private_chat_usage : "在私聊频道<br/>请采用(/用户名 聊天内容)的方式发送私聊信息，不包含括号！",
		connecting : "正在加入聊天中心，请稍候...",
		reconnect_warning : "重连失败次数过多，请稍候刷新页面重试"
	};

	this.default_channel = 1;

    /*==========================================================================
	 *
	 * 内部使用的属性，尽量不要进行外部设置
	 *
	 *
	 *==========================================================================*/

	this.title_timeid = null;

	this.newMsg = new Array();

	this.send_data = new Array();

	this.chat_str = new Array();

	this.chat_user_array = new Array();

	this.chat_str_index;

	this.reconnectCount = 1;

	this.alertCount = 0;

	this.ifReconnect = true;

	this.feedback_url = "http://cs.9wee.com/";

	this.sendName = null;

	this.isIE  = (navigator.appVersion.indexOf("MSIE") != -1) ? true : false;
	this.isWin = (navigator.appVersion.toLowerCase().indexOf("win") != -1) ? true : false;
	this.isOpera = (navigator.userAgent.indexOf("Opera") != -1) ? true : false;
}
