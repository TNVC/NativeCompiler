#include "MachineCode/Native.h"

#include <cstdio>
#include <cassert>
#include <elf.h>

namespace db::native
{

  #define WRITE_INT64(ADR, VALUE) *(int64_t *) (ADR) = (int64_t) (VALUE)

  const size_t PROGRAM_HEADERS_OFFSET = sizeof(Elf64_Ehdr);
  const size_t SECTION_HEADERS_OFFSET = 0;
  const size_t PROGRAM_HEADERS_COUNT = 3;
  const size_t SECTION_HEADERS_COUNT = 0;
  const size_t SHSTRTAB_INDEX = 0;
  const int64_t NO_FLAGS = 0;

  enum ProgramHeadersIndex { _text, _rodata, _data };

  void GenerateElf(const X86Code *code, const char *filePath)
  {
    assert(code && filePath);

    FILE *file = fopen(filePath, "wb");
    if (!file)
      {
       fprintf(stderr,
               "Fail to open \"%s\". File: \"%s\", Line: %d.\n",
               filePath, __FILE__, __LINE__);
       return;
     }


    uint64_t entryAddress =
        db::ENTRY0_ADDRESS +
        sizeof(Elf64_Ehdr) +
        sizeof(Elf64_Phdr)*PROGRAM_HEADERS_COUNT +
        sizeof(Elf64_Shdr)*SECTION_HEADERS_COUNT +
        code->flashing.mainAddress;

    struct {
      Elf64_Ehdr elfHeader;
      Elf64_Phdr programHeaders[PROGRAM_HEADERS_COUNT];
      Elf64_Phdr sectionHeaders[SECTION_HEADERS_COUNT];
    } elf
    {
      {
        { ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS64, ELFDATA2LSB, EV_CURRENT, ELFOSABI_LINUX },
        ET_EXEC,
        EM_X86_64,
        EV_CURRENT,
        entryAddress,
        PROGRAM_HEADERS_OFFSET,
        SECTION_HEADERS_OFFSET,
        NO_FLAGS,
        sizeof(Elf64_Ehdr),
        sizeof(Elf64_Phdr),
        PROGRAM_HEADERS_COUNT,
        sizeof(Elf64_Shdr),
        SECTION_HEADERS_COUNT,
        SHSTRTAB_INDEX
      }
    };

    Elf64_Off  offset  { 0x00 };
    Elf64_Addr address { ENTRY0_ADDRESS };
    Elf64_Xword size { code->text.size + sizeof(elf) };
    Elf64_Xword align { 0x1000 };
    elf.programHeaders[_text] =
        { PT_LOAD, PF_R | PF_X, offset, address, address, size, size, align };

    offset += size;
    address += size + align;
    size = code->rodata.size;
    elf.programHeaders[_rodata] =
        { PT_LOAD, PF_R       , offset, address, address, size, size, align };
    WRITE_INT64(&code->text.data[code->flashing.rodata], address);

    offset += size;
    address += size + align;
    size = code->data.size;
    elf.programHeaders[_data] =
        { PT_LOAD, PF_R | PF_W, offset, address, address, size, size, align };
    WRITE_INT64(&code->text.data[code->flashing.  data], address);

    fwrite(&elf, sizeof(elf), 1, file);
    fwrite(code->text  .data, code->text  .size, 1, file);
    fwrite(code->rodata.data, code->rodata.size, 1, file);
    fwrite(code->data  .data, code->data  .size, 1, file);

    fclose(file);
  }

}