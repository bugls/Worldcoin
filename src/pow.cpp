// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Worldcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <auxpow.h>
#include <arith_uint256.h>
#include <chain.h>
#include <worldcoin.h>
#include <primitives/block.h>
#include <uint256.h>
#include <util.h>



unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    // Dogecoin: Special rules for minimum difficulty blocks with Digishield
    if (AllowDigishieldMinDifficultyForBlock(pindexLast, pblock, params))
    {
        // Special difficulty rule for testnet:
        // If the new block's timestamp is more than 2* nTargetSpacing minutes
        // then allow mining of a min-difficulty block.
        return nProofOfWorkLimit;
    }


    int nHeight = pindexLast->nHeight + 1;

    int64_t retargetSpacing  = params.nPowTargetSpacing;
    int64_t retargetInterval = params.DifficultyAdjustmentInterval();  


    if (nHeight >= params.nDiffChangeTargetDigishield) {
        
        retargetSpacing  = params.nTargetSpacingRe;
        retargetInterval = params.DifficultyAdjustmentIntervalDigiShield();
    }
    if (nHeight >= params.nDiffChangeTarget) {
        retargetSpacing  = params.nTargetSpacingRe;
        retargetInterval = params.DifficultyAdjustmentIntervalRe();
    } else {
        //set default to pre-v6.4.3 patch values
        retargetSpacing  = params.nPowTargetSpacing;
        retargetInterval = params.DifficultyAdjustmentInterval();  
    }

    // Only change once per difficulty adjustment interval
    const int64_t difficultyAdjustmentInterval = retargetInterval;

    if ((pindexLast->nHeight+1) % difficultyAdjustmentInterval != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Litecoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = difficultyAdjustmentInterval-1;
    if ((pindexLast->nHeight+1) != difficultyAdjustmentInterval)
        blockstogoback = difficultyAdjustmentInterval;

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - blockstogoback;
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateWorldcoinNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

#if 0
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();
    int nHeight = pindexLast->nHeight + 1;

    bool fNewDifficultyProtocol = (nHeight >= params.nDiffChangeTarget);

    //set default to pre-v6.4.3 patch values
    //int64_t retargetTimespan = params.nPowTargetTimespan;
    int64_t retargetSpacing  = params.nPowTargetSpacing;
    int64_t retargetInterval = params.DifficultyAdjustmentInterval();    

    if (fNewDifficultyProtocol) {
        //retargetTimespan = params.nTargetTimespanRe;
        retargetSpacing  = params.nTargetSpacingRe;
        retargetInterval = params.DifficultyAdjustmentIntervalRe();
    }
    // Only change once per difficulty adjustment interval
    if (nHeight % retargetInterval != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + retargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    // litecoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = params.DifficultyAdjustmentInterval()-1;
    if ((pindexLast->nHeight+1) != retargetInterval)
        blockstogoback = retargetInterval;

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;

    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    int nHeight = pindexLast->nHeight + 1;
    bool fNewDifficultyProtocol = (nHeight >= params.nDiffChangeTarget);
    bool fNewDifficultyProtocolAuxpow = (nHeight >= params.nDiffChangeTargetAuxpow);
    int64_t retargetTimespan = params.nPowTargetTimespan;
    //int64_t retargetSpacing  = params.nPowTargetSpacing;
    //int64_t retargetInterval = params.DifficultyAdjustmentInterval();    

    if (fNewDifficultyProtocol) {
        retargetTimespan = params.nTargetTimespanRe;
        //retargetSpacing  = params.nTargetSpacingRe;
        //retargetInterval = params.DifficultyAdjustmentIntervalRe();
    }

    if (fNewDifficultyProtocolAuxpow) {
        if (nActualTimespan < retargetTimespan/16) nActualTimespan = retargetTimespan/16;
        if (nActualTimespan > retargetTimespan*16) nActualTimespan = retargetTimespan*16;
    } else if (fNewDifficultyProtocol) {
        if (nActualTimespan < (retargetTimespan - (retargetTimespan/10)) ) nActualTimespan = (retargetTimespan - (retargetTimespan/10));
        if (nActualTimespan > (retargetTimespan + (retargetTimespan/10)) ) nActualTimespan = (retargetTimespan + (retargetTimespan/10));   
    } else {
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
#endif

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
