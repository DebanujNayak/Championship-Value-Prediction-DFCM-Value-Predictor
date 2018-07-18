#include <cinttypes>
#include "cvp.h"
#include "mypredictor.h"
#include <cassert>
#include <cstdio>
using namespace std;

static DFCM_Predictor predictor;

// Global branch and path history
static uint64_t ghr = 0, phist = 0;

// Load/Store address history
static uint64_t addrHist = 0;

// the gash function is the Shift-Xor hash function
uint64_t gash(uint64_t a , uint64_t b, uint64_t c, uint64_t d)
{
	uint64_t output = 0;
	output ^= a;
	output ^= (b>>1);
	output ^= (c>>2);
	output ^= (d>>3);
	return output;
	
}

bool getPrediction(uint64_t seq_no, uint64_t pc, uint8_t piece, uint64_t& predicted_value) {

  
  // Calculating the index.
  // Instructions are 4-bytes, so shift PC by 2.
  uint64_t index = ((pc >> 2) ^ piece) & predictor.indexMask;



  // Accessing the lastValue, strideList, tag from the Level_1_Table.
  deque<uint64_t> strideList = predictor.Level_1_Table[index].strideList;
  uint64_t lastValue = predictor.Level_1_Table[index].lastValue;
  
  
  uint64_t a=strideList[0] & predictor.indexMask;
  uint64_t b=strideList[1] & predictor.indexMask;
  uint64_t c=strideList[2] & predictor.indexMask;
  uint64_t d=strideList[3] & predictor.indexMask;

  
  // Calculating the stride_index using the gash function
  uint64_t stride_index = gash(a,b,c,d) & predictor.indexMask;


  //Accessing the stride from the stride_Table
  uint64_t stride = predictor.Level_2_Table[stride_index].stride;

  predicted_value = lastValue + stride;
  uint8_t use_pred = (predictor.Level_2_Table[stride_index].conf >=7);
  
  // Speculate using the prediction only if confidence is high enough
  return use_pred;
}



void speculativeUpdate(uint64_t seq_no,    		// dynamic micro-instruction # (starts at 0 and increments indefinitely)
                       bool eligible,			// true: instruction is eligible for value prediction. false: not eligible.
		       uint8_t prediction_result,	// 0: incorrect, 1: correct, 2: unknown (not revealed)
		       // Note: can assemble local and global branch history using pc, next_pc, and insn.
		       uint64_t pc,			
		       uint64_t next_pc,
		       InstClass insn,
		       uint8_t piece,
		       // Note: up to 3 logical source register specifiers, up to 1 logical destination register specifier.
		       // 0xdeadbeef means that logical register does not exist.
		       // May use this information to reconstruct architectural register file state (using log. reg. and value at updatePredictor()).
		       uint64_t src1,
		       uint64_t src2,
		       uint64_t src3,
		       uint64_t dst) { 

  // we will attempt to predict ALU/LOAD/SLOWALU 
  bool isPredictable = insn == aluInstClass || insn == loadInstClass || insn == slowAluInstClass;
 
  // index is calulated and strideList is accessed.
  uint64_t index = ((pc >> 2) ^ piece) & predictor.indexMask;
  deque<uint64_t> strideList = predictor.Level_1_Table[index].strideList;


  uint64_t a=strideList[0] & predictor.indexMask;
  uint64_t b=strideList[1] & predictor.indexMask;
  uint64_t c=strideList[2] & predictor.indexMask;
  uint64_t d=strideList[3] & predictor.indexMask;

  
  // Calculating the stride_index using the gash function
  uint64_t stride_index = gash(a,b,c,d) & predictor.indexMask;


 if(isPredictable)
  {

	 // pusihng the instruction's inflightInfo into InflightPreds.
	 // increasing the inflight number for the given PC to indicate 
	 // one more instruction inflight which is yet to be committed.
	predictor.inflightPreds.push_front({seq_no, index, stride_index});
	predictor.Level_1_Table[index].inflight++;
    
	return;
  }

  
  // At this point, any branch-related information is architectural, i.e.,
  // updating the GHR/LHRs is safe.
  bool isCondBr = insn == condBranchInstClass;
  bool isIndBr = insn == uncondIndirectBranchInstClass;
  
  // Infrastructure provides perfect branch prediction.
  // As a result, the branch-related histories can be updated now.
  if(isCondBr)
    ghr = (ghr << 1) | (pc != next_pc + 4);

  if(isIndBr)
	phist = (phist << 4) | (next_pc & 0x3);
}



void updatePredictor(uint64_t seq_no,		// dynamic micro-instruction #
		     uint64_t actual_addr,	// load or store address (0xdeadbeef if not a load or store instruction)
		     uint64_t actual_value,	// value of destination register (0xdeadbeef if instr. is not eligible for value prediction)
		     uint64_t actual_latency) {//{}	// actual execution latency of instruction

  std::deque<DFCM_Predictor::InflightInfo> & inflight = predictor.inflightPreds;

  // If we have value predictions waiting for corresponding update
  if(inflight.size() && seq_no == inflight.back().seqNum)
  {     
   
	uint64_t new_stride =   actual_value - predictor.Level_1_Table[inflight.back().index].lastValue; 
	
    // If there are not other predictions corresponding to this PC in flight in the pipeline, 
    // we make sure the DFCM stride history used to predict is clean by copying the architectural value
    // history in it.

    if(--predictor.Level_1_Table[inflight.back().index].inflight == 0)
    {
		predictor.Level_1_Table[inflight.back().index].strideList.pop_back();	
		predictor.Level_1_Table[inflight.back().index].strideList.push_front(predictor.Level_1_Table[inflight.back().index].lastValue);	
		predictor.Level_1_Table[inflight.back().index].lastValue = actual_value;
    }

	// If the predicted stride is equal to new stride , increase confidence.
    if(predictor.Level_2_Table[inflight.back().stride_Index].stride == new_stride)
	{
	     predictor.Level_2_Table[inflight.back().stride_Index].conf = 
	     std::min(predictor.Level_2_Table[inflight.back().stride_Index].conf + 1,10);
    }

	// if confidence is zero ,change the stride value in the predictor to new_stride.
    else if(predictor.Level_2_Table[inflight.back().stride_Index].conf == 0)
      predictor.Level_2_Table[inflight.back().stride_Index].stride = new_stride;

	// If prediction was wrong ,but confidence is not zero , then decrease confidence.
    else
      predictor.Level_2_Table[inflight.back().stride_Index].conf--;
		
    inflight.pop_back();
  }



  // It is now safe to update the address history register
  //if(insn == loadInstClass || insn == storeInstClass) 
  if(actual_addr != 0xdeadbeef)
   addrHist = (addrHist << 4) | actual_addr;
}


void beginPredictor(int argc_other, char **argv_other) {
   if (argc_other > 0)
      printf("CONTESTANT ARGUMENTS:\n");

   for (int i = 0; i < argc_other; i++)
      printf("\targv_other[%d] = %s\n", i, argv_other[i]);
}

void endPredictor() {
	printf("CONTESTANT OUTPUT--------------------------\n");
	printf("--------------------------\n");
}
