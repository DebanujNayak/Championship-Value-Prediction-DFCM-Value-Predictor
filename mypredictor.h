#include <vector>
#include <deque>
#include <queue>
using namespace std;


struct DFCM_Predictor
{	
	struct FirstLevelEntry
	{
		uint64_t lastValue;
		deque<uint64_t>strideList;
		uint64_t inflight;

		FirstLevelEntry() :lastValue(0), inflight(0), strideList(deque<uint64_t>(4,0)){}
	};

	vector<FirstLevelEntry> Level_1_Table;


	struct SecondLevelEntry
	{
		uint64_t stride;
		uint8_t conf;

		SecondLevelEntry() : stride(0), conf(0) {}
	};
	
	vector<SecondLevelEntry> Level_2_Table;
	
	
	struct InflightInfo
	{
		uint64_t seqNum;
		uint64_t index;
		uint64_t stride_Index;

		InflightInfo() : seqNum(0), index(0) {}
		InflightInfo(uint64_t sn, uint64_t idx, uint64_t str_idx) : seqNum(sn), index(idx), stride_Index(str_idx) {}


	};

	deque<InflightInfo> inflightPreds;
	
	uint64_t globalHistoryRegister;
  	uint64_t pathHistoryRegister;
  	uint64_t addressHistoryRegister;

	uint64_t indexMask;

public:
	
DFCM_Predictor() : globalHistoryRegister(0), pathHistoryRegister(0), addressHistoryRegister(0)
{
	Level_1_Table.resize(4096 * 4);
	Level_2_Table.resize(4096 * 4);
	indexMask = (4096 *4)-1;
	
}
};
	
	
		
