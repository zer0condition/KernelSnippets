void VGK_WhitelistThread_SwapContextHk(uint64_t CurrentThread) 
{
    uint64_t currentCR3 = __readcr3();
    if (currentCR3 != GameProcessCR3) {
        return; 
    }

    // check if the current thread is whitelisted to allow memory context switching.
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

    // prepare a new CR3 context for the whitelisted thread
    _disable(); 

    // copy the current game CR3 page directory to the cloned CR3.
    custom_memcpy(reinterpret_cast<void*>(ClonedCR3), reinterpret_cast<void*>(GameProcessCR3), 0x1000);

    // set up a shadow page table entry for the cloned CR3.
    *reinterpret_cast<uint64_t*>(ClonedCR3 + 8 * FreePML4Index) = ShadowPageTableEntry;

    // write the cloned CR3 if the thread is allowed to change memory context
    if (isThreadAllowedToWriteCR3) {
        __writecr3(ClonedCR3);
    }

    // iterate over the page table array and clear access permissions based on the access byte
    uint64_t clientPageTable = reinterpret_cast<uint64_t>(PageTableArray);
    uint64_t adjustedAccessAddress = GameProcessCR3 ^ PageTableArray[PageAccessByte];

    // loop through each PML4 entry
    for (uint64_t i = 0; i < 512; ++i) { 
        uint64_t entry = *reinterpret_cast<uint64_t*>(clientPageTable + 8 * i);
        // check if the page table entry is valid
        if ((entry & 1) != 0) {
            uint64_t entryIndex = (entry >> 12) & 0xFFFFFFFFF;
            *reinterpret_cast<uint64_t*>(adjustedAccessAddress + 48 * entryIndex) &= ~1ULL; // clear access bit
        }
    }

    // flush the tlb if required
    if (ShouldFlushTLB) {
        uint64_t originalCR4 = __readcr4();
        __writecr4(originalCR4 ^ 0x80);
        __writecr4(originalCR4); 
    }

    _enable();
}
