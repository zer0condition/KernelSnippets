void VGK_WhitelistThread_SwapContextHk(uint64_t CurrentThread) 
{
    uint64_t currentCR3 = __readcr3();
    if (currentCR3 != GameProcessCR3) {
        return; 
    }
 
    // Check if the current thread is whitelisted to allow memory context switching
    bool isThreadAllowedToWriteCR3 = false;
    AcquireSpinLock();
    if (WhitelistedThreadCount > 0) {
        for (uint32_t i = 0; i < WhitelistedThreadCount; ++i) {
            if (CurrentThread == *reinterpret_cast<uint64_t*>(WhitelistedThreadsArray + 8 * i)) {
                isThreadAllowedToWriteCR3 = true;
                break;
            }
        }
    }
    ReleaseSpinLock();
 
    if (!isThreadAllowedToWriteCR3) {
        return;
    }
 
    // Prepare a new CR3 context for the whitelisted thread
    _disable(); 
 
    // copy the current game CR3 page directory to the cloned CR3
    custom_memcpy(reinterpret_cast<void*>(ClonedCR3), reinterpret_cast<void*>(GameProcessCR3), 0x1000);
 
    // set up a shadow page table entry for the cloned CR3
    *reinterpret_cast<uint64_t*>(ClonedCR3 + 8 * FreePML4Index) = Data::ShadowPML4Value;
 
    // write the cloned CR3 if the thread is allowed to change memory context; 
    if (isThreadAllowedToWriteCR3) {
        __writecr3(DecryptedClonedCR3); // decryption routine is redacted to make it readable; v17 and the one inside v3 condition also works.
    }
 
    // loop through each entry in the OriginalPML4 (ShadowRegionsDataStructure)
    uint64_t OriginalPML4_t = reinterpret_cast<uint64_t>(Data::OriginalPML4/*qword_83D18*/);
    uint64_t MMPfnDatabase = qword_140080AF0 ^ qword_140080AF8[byte_80AE9];  // calculate MMPfnDatabase based on provided XOR
    
    for (uint64_t i = 0; i < 0x200; ++i) {
        uint64_t entry = *reinterpret_cast<uint64_t*>(OriginalPML4_t + 8 * i); // access each entry
        // check if the LSB is set
        if ((entry & 1) != 0) {  
            // calculate the corresponding PFN index and modify its MMPFN entry
            uint64_t entryIndex = (entry >> 12) & 0xFFFFFFFFF;
            *reinterpret_cast<uint64_t*>(MMPfnDatabase + 48 * entryIndex) &= ~1ULL; // clear the active bit
        }
    }
 
    // flush TLB if required
    if (ShouldFlushTLB) {
        uint64_t originalCR4 = __readcr4();
        __writecr4(originalCR4 ^ 0x80); 
        __writecr4(originalCR4);    
    }
 
    _enable();
}
