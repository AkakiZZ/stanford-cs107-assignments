#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

#include "teller.h"
#include "account.h"
#include "error.h"
#include "debug.h"
#include "branch.h"

/*
 * deposit money into an account
 */
int
Teller_DoDeposit(Bank *bank, AccountNumber accountNum, AccountAmount amount)
{
  assert(amount >= 0);

  DPRINTF('t', ("Teller_DoDeposit(account 0x%"PRIx64" amount %"PRId64")\n",
                accountNum, amount));

  Account *account = Account_LookupByNumber(bank, accountNum);

  if (account == NULL) {
    return ERROR_ACCOUNT_NOT_FOUND;
  }
  BranchID id = (BranchID) (accountNum >> 32);
  sem_wait(&(account->accountLock));
  sem_wait(&(bank->branches[id].lock));
  Account_Adjust(bank,account, amount, 1);
  sem_post(&(account->accountLock));
  sem_post(&(bank->branches[id].lock));
  return ERROR_SUCCESS;
}

/*
 * withdraw money from an account
 */
int
Teller_DoWithdraw(Bank *bank, AccountNumber accountNum, AccountAmount amount)
{
  assert(amount >= 0);

  DPRINTF('t', ("Teller_DoWithdraw(account 0x%"PRIx64" amount %"PRId64")\n",
                accountNum, amount));

  Account *account = Account_LookupByNumber(bank, accountNum);

  if (account == NULL) {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  BranchID id = (BranchID) (accountNum >> 32);

  sem_wait(&(account->accountLock));
  sem_wait(&(bank->branches[id].lock));

  if (amount > Account_Balance(account)) {
    sem_post(&(account->accountLock));
    sem_post(&(bank->branches[id].lock));
    return ERROR_INSUFFICIENT_FUNDS;
  }

  Account_Adjust(bank,account, -amount, 1);

  sem_post(&(account->accountLock));
  sem_post(&(bank->branches[id].lock));
  return ERROR_SUCCESS;
}

/*
 * do a tranfer from one account to another account
 */
int
Teller_DoTransfer(Bank *bank, AccountNumber srcAccountNum,
                  AccountNumber dstAccountNum,
                  AccountAmount amount)
{
  assert(amount >= 0);

  DPRINTF('t', ("Teller_DoTransfer(src 0x%"PRIx64", dst 0x%"PRIx64
                ", amount %"PRId64")\n",
                srcAccountNum, dstAccountNum, amount));

  Account *srcAccount = Account_LookupByNumber(bank, srcAccountNum);
  if (srcAccount == NULL) {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  Account *dstAccount = Account_LookupByNumber(bank, dstAccountNum);
  if (dstAccount == NULL) {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  if(srcAccount == dstAccount)  return ERROR_SUCCESS;

  BranchID sourceID = (BranchID) (srcAccountNum >> 32);;
  BranchID destID = (BranchID) (dstAccountNum >> 32);;

  if (sourceID == destID) {
    if(srcAccount->accountNumber < dstAccount->accountNumber) {
      sem_wait(&(srcAccount->accountLock));
      sem_wait(&(dstAccount->accountLock));
    } else {
      sem_wait(&(dstAccount->accountLock));
      sem_wait(&(srcAccount->accountLock));
    }
  } else {
    if (sourceID < destID) { 
      sem_wait(&(srcAccount->accountLock));
      sem_wait(&(dstAccount->accountLock));
      sem_wait(&(bank->branches[sourceID].lock));
      sem_wait(&(bank->branches[destID].lock));
    } else {
      sem_wait(&(dstAccount->accountLock));
      sem_wait(&(srcAccount->accountLock));
      sem_wait(&(bank->branches[destID].lock));
      sem_wait(&(bank->branches[sourceID].lock));
    }
  }

  if (amount > Account_Balance(srcAccount)) {
    sem_post(&(srcAccount->accountLock));
    sem_post(&(dstAccount->accountLock));
    if (sourceID != destID) {
      sem_post(&(bank->branches[sourceID].lock));
      sem_post(&(bank->branches[destID].lock));
    } 
    return ERROR_INSUFFICIENT_FUNDS;
  }

  /*
   * If we are doing a transfer within the branch, we tell the Account module to
   * not bother updating the branch balance since the net change for the
   * branch is 0.
   */
  int updateBranch = !Account_IsSameBranch(srcAccountNum, dstAccountNum);

  Account_Adjust(bank, srcAccount, -amount, updateBranch);
  Account_Adjust(bank, dstAccount, amount, updateBranch);

  sem_post(&(srcAccount->accountLock));
  sem_post(&(dstAccount->accountLock));
  if (sourceID != destID) {
    sem_post(&(bank->branches[sourceID].lock));
    sem_post(&(bank->branches[destID].lock));
  } 
  return ERROR_SUCCESS;
}
