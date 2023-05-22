#include "CodeGen/x86Code.h"

#include "Utils/ErrorMessage.h"

#include <elf.h>
#include <cassert>

namespace db {

  #define WRITE_INT64(ADR, VALUE) *(int64_t *) (ADR) = (int64_t) (VALUE)

  const size_t PROGRAM_HEADERS_COUNT  = 4;
  const size_t PROGRAM_HEADERS_OFFSET = sizeof(Elf64_Ehdr);

  const size_t ENTRY_ADDRESS = 0x400000;

  struct Headers {
    Elf64_Ehdr elfHeader;
    Elf64_Phdr programHeaders[PROGRAM_HEADERS_COUNT];
  };

  enum ProgramHeadersIndex { _text = 0, _stdlib = 1, _rodata = 2, _data = 3 };

  bool GenerateELF(const x86Code *code, const char *filePath)
    {
      assert(code && filePath);

      FILE *file = fopen(filePath, "wb");
      if (!file) FAIL_TO_OPEN(filePath, return false);

      uint64_t entryAddress =
          ENTRY_ADDRESS + sizeof(Headers) + code->mainOffset;
      Headers headers =
          {
            {
              { ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS64, ELFDATA2LSB, EV_CURRENT, ELFOSABI_LINUX },
              ET_EXEC,
              EM_X86_64,
              EV_CURRENT,
              entryAddress,
              PROGRAM_HEADERS_OFFSET,
              0,
              0,
              sizeof(Elf64_Ehdr),
              sizeof(Elf64_Phdr),
              PROGRAM_HEADERS_COUNT,
              sizeof(Elf64_Shdr),
              0,
              0
            }
          };

      Elf64_Off  offset  { 0x00 };
      Elf64_Addr address { ENTRY_ADDRESS };
      Elf64_Xword size { code->text.size - code->stdlib.size + sizeof(headers) };
      Elf64_Xword align { 0x1000 };
      headers.programHeaders[_text] =
          { PT_LOAD, PF_R | PF_X       , offset, address, address, size, size, align };

      offset  += size;
      address += size;
      size = code->stdlib.size;
      headers.programHeaders[_stdlib] =
          { PT_LOAD, PF_R | PF_W | PF_X, offset, address, address, size, size, align };

      offset  += size;
      address += size + align;
      size = code->rodata.size;
      headers.programHeaders[_rodata] =
          { PT_LOAD, PF_R              , offset, address, address, size, size, align };
      WRITE_INT64(&code->text.data[code->references.writingRodataOffset], address);

      offset  += size;
      address += size + align;
      size = code->data.size;
      headers.programHeaders[_data] =
          { PT_LOAD, PF_R | PF_W       , offset, address, address, size, size, align };
      WRITE_INT64(&code->text.data[code->references.writingDataOffset]  , address);

      fwrite(&headers, sizeof(headers), 1, file);
      fwrite(code->text  .data, code->text  .size, 1, file);
      fwrite(code->rodata.data, code->rodata.size, 1, file);
      fwrite(code->data  .data, code->data  .size, 1, file);

      fclose(file);
      return true;
    }

}