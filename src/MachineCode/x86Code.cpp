#include "MachineCode/x86Code.h"

#include "Utils/ErrorMessage.h"

#include <cmath>
#include <malloc.h>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <sys/mman.h>

namespace db
{

  const unsigned char PROLOG[] =
      {
          0x50, /*push rax*/
          0x53, /*push rbx*/
          0x51, /*push rcx*/
          0x52, /*push rdx*/
          0x56, /*push rsi*/
          0x57, /*push rdi*/
          0x41, 0x56, /*push r14*/
          0x41, 0x57, /*push r15*/
          0x41, 0x52, /*push r10*/
          0x49, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*movabs r10, 0x0*/
      };
  const size_t RAM_ADDRESS_REGISTER_POSITION = 14;

  const size_t PAGE_SIZE = 4096;
  const size_t MAX_PAGE_COUNT_FOR_CODE = 4;
  const size_t PAGE_COUNT_FOR_RAM = 3;

  typedef size_t oldPosition;
  typedef size_t newPosition;

  struct Position {
    oldPosition oldPos;
    newPosition newPos;
  };

  struct Reference {
    newPosition position;
    newPosition referencePosition;
    oldPosition refereePosition;
    size_t delta;
  };

  struct ReferenceTable {
    size_t size;
    size_t capacity;
    Reference *data;
    Position *positionData;
  };

  struct ExternalReferenceTable {
    size_t size;
    size_t capacity;
    Reference *data;
  };

  static bool AddReference(ReferenceTable *table,
                           newPosition position,
                           newPosition referencePos,
                           oldPosition refereePos,
                           size_t delta);
  static bool UpdataReferences(X86Code *code,
                               ReferenceTable *table,
                               oldPosition oldPos,
                               newPosition newPos);
  static bool UpdataReferences(X86Code *code,
                               ReferenceTable *table);


  static bool AddReference(ExternalReferenceTable *table,
                           newPosition position,
                           newPosition referencePos,
                           oldPosition externalPos,
                           size_t delta);
  static bool UpdataReferences(X86Code *code,
                               ExternalReferenceTable *table,
                               newPosition startPosition);

  static bool AddReference(ReferenceTable *table,
                           newPosition position,
                           newPosition referencePos,
                           oldPosition refereePos,
                           size_t delta)
  {
    assert(table);

    if (table->size == table->capacity)
      {
        size_t newCapacity =
            table->capacity*2 + 1;
        Reference *temp =
            (Reference *) realloc(table->data, newCapacity*sizeof(Reference));
        if (!temp)
          OUT_OF_MEMORY(return false);
        table->data = temp;
        table->capacity = newCapacity;
      }

    table->data[table->size++] =
        { position, referencePos, refereePos, delta };

    return true;
  }

  static bool UpdataReferences(X86Code *code,
                               ReferenceTable *table,
                               oldPosition oldPos,
                               newPosition newPos)
  {
    assert(code && table);

    table->positionData[oldPos] = { oldPos, newPos };

    return true;
  }

  static bool UpdataReferences(X86Code *code,
                               ReferenceTable *table)
  {
    assert(code && table);

  for (size_t i = 0; i < table->size; ++i)
    {
      newPosition newPos =
          table->positionData[table->data[i].refereePosition].newPos;
      Reference *ref =  &table->data[i];
      size_t address = newPos - ref->referencePosition + ref->delta;
      *((int32_t *) &code->text.data[ref->referencePosition]) =
          int32_t(address);
    }

    return true;
  }


  static bool AddReference(ExternalReferenceTable *table,
                           newPosition position,
                           newPosition referencePos,
                           oldPosition externalPos,
                           size_t delta)
  {
    assert(table);

    if (table->size == table->capacity)
      {
        size_t newCapacity =
            table->capacity*2 + 1;
        Reference *temp =
            (Reference *) realloc(table->data, newCapacity*sizeof(Reference));
        if (!temp)
          OUT_OF_MEMORY(return false);
        table->data = temp;
        table->capacity = newCapacity;
      }

    table->data[table->size++] =
        { position, referencePos, externalPos, delta };

    return true;
  }

  static bool UpdataReferences(X86Code *code,
                               ExternalReferenceTable *table,
                               newPosition startPosition)
  {
    assert(code && table);

    for (size_t i = 0; i < table->size; ++i)
      {
        Reference *ref =  &table->data[i];
        size_t address = ref->refereePosition - ref->referencePosition - startPosition + ref->delta;
        *((int32_t *) &code->text.data[ref->referencePosition]) =
            int32_t(address);
      }

    return true;
  }


  void Write(X86Code *code, const void *buffer, size_t size)
    {
      assert(code && buffer && size);

      if (code->text.size + size >= code->text.capacity)
        {
          size_t newCapacity = code->text.capacity*2 + size + 1;
          std::byte *temp =
              (std::byte *) realloc(code->text.data, newCapacity);
          if (!temp)
            OUT_OF_MEMORY(return);
          code->text.data = temp;
          code->text.capacity = newCapacity;
        }

      memcpy((char *) code->text.data + code->text.size, (const char *) buffer, size);
      code->text.size += size;
    }
  void DestroyX86Code(X86Code *code)
  {
    assert(code);

    if (code->isJIT)
      munmap(code->text.data, code->text.capacity);
    else
      free(code->text.data);
    free(code->data  .data);
    free(code->rodata.data);
    free(code);
  }

}