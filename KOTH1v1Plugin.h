#pragma once
#pragma comment( lib, "bakkesmod.lib" )
#include "bakkesmod/plugin/bakkesmodplugin.h"

#define VERSION "0.22"
#define DEBUG 1

struct Popup
{
	float startTime = 0.f;
	std::string text = "";
	Vector2 startLocation = { -1, -1 };
};

class KOTH1v1Plugin : public BakkesMod::Plugin::BakkesModPlugin
{
private:
	map<unsigned long long, int> goalMap;
	unsigned long long lastTouchID = 0LL;
	int kingTeamID;
	int notKingTeamID;

	void debugLog(string);
	int getTeamNumberByName(string);
	void initializeTeamIDs();
	void hookIfKOTH(string);
	void unhookKOTHEvents(string);
	void cachePlayerScores(bool);
	PriWrapper* findSelfPri(ServerWrapper);
	PriWrapper* getLastGoalPri();
	void changeTeam(PriWrapper*);
public:
	virtual void OnLoad();
	virtual void OnUnload();
	void OnScoreUpdated(string);
	void OnHitBall(string);
	void OnLastGoal();
	void SpawnSpectator(string);
};

