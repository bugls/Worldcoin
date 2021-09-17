// Copyright (c) 2015-2017 The Worldcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/random/uniform_int.hpp>
#include <boost/random/mersenne_twister.hpp>

#include <arith_uint256.h>
#include <worldcoin.h>
#include <pow.h>
#include <util.h>

// As Worldcoin calculate block reward using trigonometric functions to follow the Earth rotation ...
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

int static generateMTRandom(unsigned int s, int range)
{
    boost::mt19937 gen(s);
    boost::uniform_int<> dist(1, range);
    return dist(gen);
}

// Worldcoin: Normally minimum difficulty blocks can only occur in between
// retarget blocks. However, once we introduce Digishield every block is
// a retarget, so we need to handle minimum difficulty on all blocks.
//bool AllowDigishieldMinDifficultyForBlock(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
//{
//   // never allow minimal difficulty, WDC retarget works fine	
//    return false;
//}

//unsigned int CalculateWorldcoinNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
//{
//    // do not change retarget algo	
//    return CalculateNextWorkRequired(pindexLast, nFirstBlockTime, params);
//}

bool CheckAuxPowProofOfWork(const CBlockHeader& block, const Consensus::Params& params)
{
    // check legacy blocks and non-AUXPOW
    if ( block.IsLegacy() || !block.IsAuxpow() ) {
        if (block.auxpow)
            return error("%s : legacy block with auxpow properties",
                         __func__);
        return CheckProofOfWork(block.GetPoWHash(), block.nBits, params);
    }

    /* We have auxpow.  Check it.  */
    if (!block.auxpow)
            return error("%s : auxpow block without auxpow properties",
                         __func__);

    /* Except for legacy blocks with full version 1, ensure that
       the chain ID is correct.  Legacy blocks are not allowed since
       the merge-mining start, which is checked in AcceptBlockHeader
       where the height is known.  */
    if (params.fStrictChainId && (block.GetChainId() != params.nAuxpowChainId) )
        return error("%s : block does not have our chain ID"
                     " (got %d, expected %d, full nVersion %d)",
                     __func__, block.GetChainId(),
                     params.nAuxpowChainId, block.nVersion);

    if (!block.auxpow->check(block.GetHash(), block.GetChainId(), params))
        return error("%s : AUX POW is not valid", __func__);
    if (!CheckProofOfWork(block.auxpow->getParentBlockPoWHash(), block.nBits, params))
        return error("%s : AUX proof of work failed", __func__);

    return true;	
}

bool AllowDigishieldMinDifficultyForBlock(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    // check if the chain allows minimum difficulty blocks
    if (!params.fPowAllowMinDifficultyBlocks)
        return false;

    // check if the chain allows minimum difficulty blocks on recalc blocks
    if (pindexLast->nHeight < params.nDiffChangeTargetDigishield)
    // if (!params.fPowAllowDigishieldMinDifficultyBlocks)
        return false;

    // Allow for a minimum block time if the elapsed time > 2*nTargetSpacing
    return (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2);
}



unsigned int CalculateWorldcoinNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    int nHeight = pindexLast->nHeight + 1;
    int64_t retargetTimespan = params.nPowTargetTimespan;
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    int64_t nModulatedTimespan = nActualTimespan;
    int64_t nMaxTimespan;
    int64_t nMinTimespan;

    bool fNewDifficultyProtocol = (nHeight >= params.nDiffChangeTarget);
    bool fNewDifficultyProtocolAuxpow = (nHeight >= params.nDiffChangeTargetAuxpow);
    bool fNewDifficultyProtocolDigiShiled = (nHeight >= params.nDiffChangeTargetDigishield);


    if (fNewDifficultyProtocolDigiShiled) { //DigiShield implementation - thanks to RealSolid & WDC for this code
        retargetTimespan = params.nDigishieldPowTargetTimespan;
        // amplitude filter - thanks to daft27 for this code
        nModulatedTimespan = retargetTimespan + (nModulatedTimespan - retargetTimespan) / 8;
        nMinTimespan = retargetTimespan - (retargetTimespan / 4);
        nMaxTimespan = retargetTimespan + (retargetTimespan / 2);

        // Limit adjustment step
        if (nModulatedTimespan < nMinTimespan)
            nModulatedTimespan = nMinTimespan;
        else if (nModulatedTimespan > nMaxTimespan)
            nModulatedTimespan = nMaxTimespan;

        // Retarget
        const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
        arith_uint256 bnNew;
        arith_uint256 bnOld;
        bnNew.SetCompact(pindexLast->nBits);
        bnOld = bnNew;
        bnNew *= nModulatedTimespan;
        bnNew /= retargetTimespan;

        if (bnNew > bnPowLimit)
            bnNew = bnPowLimit;

        return bnNew.GetCompact();
        
    } else if (fNewDifficultyProtocolAuxpow) {
        retargetTimespan = params.nTargetTimespanRe;
        if (nActualTimespan < retargetTimespan/16) nActualTimespan = retargetTimespan/16;
        if (nActualTimespan > retargetTimespan*16) nActualTimespan = retargetTimespan*16;
    } else if (fNewDifficultyProtocol) {
        retargetTimespan = params.nTargetTimespanRe;
        if (nActualTimespan < (retargetTimespan - (retargetTimespan/10)) ) nActualTimespan = (retargetTimespan - (retargetTimespan/10));
        if (nActualTimespan > (retargetTimespan + (retargetTimespan/10)) ) nActualTimespan = (retargetTimespan + (retargetTimespan/10));  
    } else {
        retargetTimespan = params.nPowTargetTimespan;
        if (nActualTimespan < retargetTimespan/4) nActualTimespan = retargetTimespan/4;
        if (nActualTimespan > retargetTimespan*4) nActualTimespan = retargetTimespan*4;
    }

    // Retarget
    arith_uint256 bnNew;
    arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    // litecoin: intermediate uint256 can overflow by 1 bit
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    bool fShift = bnNew.bits() > bnPowLimit.bits() - 1;
    if (fShift)
        bnNew >>= 1;
    bnNew *= nActualTimespan;
    bnNew /= retargetTimespan;
    if (fShift)
        bnNew <<= 1;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

CAmount GetWorldcoinBlockSubsidy(int nHeight, const Consensus::Params& consensusParams)
{
    const CAmount nMinSubsidy = 1 * COIN;
    // base payout
    CAmount nSubsidy = 10000 * COIN;

    // first block receives 2% premine to support WorldCoin
    if (nHeight == 1) {
	    nSubsidy = 270000000LL * COIN;
	    return nSubsidy;
    }

    // relative height inside main period (approx. 1 year)
    int nHeightM = nHeight % 525600;
    double phase = ((double)nHeightM) / 525600.0 * 2.0 * M_PI;
    // modify base payout with seasonal effect
    nSubsidy += ((int)(2000.0 * sin(phase))) * COIN;

    // bonus zones

    // get number of days since the inception of WorldCoin
    int day = nHeight / 1440 + 1;

    // regular bonus zones

    // every 31 days, payout is increased by a factor of 5
    if (day % 31 == 0) {
	    nSubsidy *= 5;
    }
    // every 14 days, payout is increased by a factor of 2
    else if (day % 14 == 0) {
	    nSubsidy *= 2;
    }

    // special bonus zones

    // the first three days were special (approx. 12/21-21/24 in the year of 2013)
    switch (day) {
        // 5 times the normal payout on day 1
	case 1:
	    nSubsidy *= 5;
	    break;
	// 3 times the normal payout on day 2
	case 2:
	    nSubsidy *= 3;
	    break;
	// 2 times the normal payout on day 3
	case 3:
	    nSubsidy *= 2;
	    break;
    }

    // subsidy is cut in half every 525600 blocks,
    // which will occur approximately every 12 months
    nSubsidy >>= (nHeight / 525600);

    // nevertheless, there will a minimum payout of 1 WorldCoin
    if (nSubsidy < nMinSubsidy) {
	    nSubsidy = nMinSubsidy;
    }

    return nSubsidy;
}
