#include "pch.h"
#include "Command.h"

CCommand::CCommand() :threadid(0) {
	struct {
		int nCmd;
		CMDFUNC func;
	}data[] = {
		{1, &CCommand::MakeDriverInfo},
		{2, &CCommand::MakeDirecteryInfo},
		{3, &CCommand::RunFIle},
		{4, &CCommand::DownloadFile},
		{5, &CCommand::MouseEvent},
		{6, &CCommand::SendScreen},
		{7, &CCommand::LockMachine},
		{8, &CCommand::UnlockMachine},
		{9, &CCommand::DeleteLocalFile},
		{1981, &CCommand::TestConnect},
		{-1, NULL}
	};

	//因为cmd有点多，switch不够优雅~ 所以用到映射表，而且目前cmd不代表以后没有新增功能
	for (int i = 0; data[i].nCmd != -1; i++) {
		m_mapFunction.insert(std::pair<int, CMDFUNC>(data[i].nCmd, data[i].func));
	}
}

int CCommand::ExcuteCommand(int nCmd, std::list<CPacket>& lstPacket, CPacket& inPacket) {
	std::map<int, CMDFUNC>::iterator it = m_mapFunction.find(nCmd);
	if (it == m_mapFunction.end()) {
		return -1;
	}
	return (this->*it->second)(lstPacket, inPacket);
}

