#define PAGE_OFFSET_MASK 0xFFF
#define PAGE_SIZE 0x1000
#define PML4_SHIFT 39
#define PDP_SHIFT 30
#define PD_SHIFT 21
#define PT_SHIFT 12
#define PML4_MASK 0x1FF
#define PDP_MASK 0x1FF
#define PD_MASK 0x1FF
#define PT_MASK 0x1FF

ULONG_PTR ReadPhysicalAddress(ULONG_PTR Address) {
    PHYSICAL_ADDRESS pa;
    pa.QuadPart = Address;
    PVOID Mapped = MmMapIoSpace(pa, PAGE_SIZE, MmNonCached);
    if (!Mapped) return 0;

    ULONG_PTR value = *(volatile ULONG_PTR*)Mapped;
    MmUnmapIoSpace(Mapped, PAGE_SIZE);
    return value;
}

BOOLEAN MmIsAddressValid(PVOID VirtualAddress, ULONG_PTR CustomCr3 = 0) {
    ULONG_PTR address = (ULONG_PTR)VirtualAddress;
    ULONG_PTR cr3 = CustomCr3 ? CustomCr3 : __readcr3();

    ULONG_PTR pml4Index = (address >> PML4_SHIFT) & PML4_MASK;
    ULONG_PTR pdpIndex = (address >> PDP_SHIFT) & PDP_MASK;
    ULONG_PTR pdIndex = (address >> PD_SHIFT) & PD_MASK;
    ULONG_PTR ptIndex = (address >> PT_SHIFT) & PT_MASK;

    ULONG_PTR pml4Base = cr3 & ~PAGE_OFFSET_MASK;
    ULONG_PTR pml4Entry = ReadPhysicalAddress(pml4Base + (pml4Index * sizeof(ULONG_PTR)));
    if (!(pml4Entry & 1)) return FALSE;

    ULONG_PTR pdpBase = pml4Entry & ~PAGE_OFFSET_MASK;
    ULONG_PTR pdpEntry = ReadPhysicalAddress(pdpBase + (pdpIndex * sizeof(ULONG_PTR)));
    if (!(pdpEntry & 1)) return FALSE;

    ULONG_PTR pdBase = pdpEntry & ~PAGE_OFFSET_MASK;
    ULONG_PTR pdEntry = ReadPhysicalAddress(pdBase + (pdIndex * sizeof(ULONG_PTR)));
    if (!(pdEntry & 1)) return FALSE;

    ULONG_PTR ptBase = pdEntry & ~PAGE_OFFSET_MASK;
    ULONG_PTR ptEntry = ReadPhysicalAddress(ptBase + (ptIndex * sizeof(ULONG_PTR)));
    if (!(ptEntry & 1)) return FALSE;

    return TRUE;
}
