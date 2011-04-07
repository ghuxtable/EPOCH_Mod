#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdf_common.h"
#ifdef PARALLEL
#include <mpi.h>
#endif

static inline int sdf_get_next_block(sdf_file_t *h)
{
    if (h->blocklist) {
        if (!h->current_block)
            h->current_block = h->blocklist;
        else if (h->current_block->next)
            h->current_block = h->current_block->next;
        else {
            sdf_block_t *block = malloc(sizeof(sdf_block_t));
            memset(block, 0, sizeof(sdf_block_t));
            block->block_start = h->current_block->next_block_location;
            h->current_block->next = block;
            h->current_block = block;
        }
    } else {
        sdf_block_t *block = malloc(sizeof(sdf_block_t));
        memset(block, 0, sizeof(sdf_block_t));
        block->block_start = h->summary_location;
        h->blocklist = h->current_block = block;
    }

    return 0;
}



int sdf_abort(sdf_file_t *h)
{
#ifdef PARALLEL
    MPI_Abort(h->comm, 1);
#endif
    _exit(1);
    return 0;
}



int sdf_seek(sdf_file_t *h)
{
#ifdef PARALLEL
    return MPI_File_seek(h->filehandle, h->current_location, MPI_SEEK_SET);
#else
    return fseeko(h->filehandle, h->current_location, SEEK_SET);
#endif
}


int sdf_read_bytes(sdf_file_t *h, char *buf, int buflen)
{
#ifdef PARALLEL
    return MPI_File_read(h->filehandle, buf, buflen, MPI_BYTE,
            MPI_STATUS_IGNORE);
#else
    return fread(buf, buflen, 1, h->filehandle);
#endif
}



int sdf_broadcast(sdf_file_t *h, void *buf, int size)
{
#ifdef PARALLEL
    return MPI_Bcast(buf, size, MPI_BYTE, h->rank_master, h->comm);
#else
    return 0;
#endif
}


int sdf_read_header(sdf_file_t *h)
{
    int buflen;

    sdf_indent = 0;

    if (h->done_header) return 1;

    buflen = SDF_HEADER_LENGTH;
    h->buffer = malloc(buflen);

    h->current_location = h->start_location = 0;

    if (h->rank == h->rank_master) {
        sdf_seek(h);
        sdf_read_bytes(h, h->buffer, buflen);
    }
    sdf_broadcast(h, h->buffer, buflen);

    // If this isn't SDF_MAGIC then this isn't a SDF file;
    if (memcmp(h->buffer, SDF_MAGIC, 4) != 0) {
        sdf_close(h);
        return -1;
    }

    h->current_location += 4;

    SDF_READ_ENTRY_INT4(h->endianness);

    SDF_READ_ENTRY_INT4(h->file_version);
    if (h->file_version > SDF_VERSION) {
        sdf_close(h);
        return -1;
    }

    SDF_READ_ENTRY_INT4(h->file_revision);

    SDF_READ_ENTRY_ID(h->code_name);

    SDF_READ_ENTRY_INT8(h->first_block_location);

    SDF_READ_ENTRY_INT8(h->summary_location);

    SDF_READ_ENTRY_INT4(h->summary_size);

    SDF_READ_ENTRY_INT4(h->nblocks);

    SDF_READ_ENTRY_INT4(h->block_header_length);

    SDF_READ_ENTRY_INT4(h->step);

    SDF_READ_ENTRY_REAL8(h->time);

    SDF_READ_ENTRY_INT4(h->jobid1);

    SDF_READ_ENTRY_INT4(h->jobid2);

    SDF_READ_ENTRY_INT4(h->string_length);

    SDF_READ_ENTRY_INT4(h->code_io_version);

    SDF_READ_ENTRY_LOGICAL(h->restart_flag);

    SDF_READ_ENTRY_LOGICAL(h->other_domains);

    free(h->buffer);
    h->buffer = NULL;

    h->current_location = SDF_HEADER_LENGTH;
    h->done_header = 1;

    return 0;
}



// Read the block header into the current block
int sdf_read_next_block_header(sdf_file_t *h)
{
    sdf_block_t *b;
    int i;

    if (!h->done_header) {
        if (h->rank == h->rank_master) {
            fprintf(stderr, "*** ERROR ***\n");
            fprintf(stderr, "SDF header has not been read. Ignoring call.\n");
        }
        return 1;
    }

    sdf_get_next_block(h);
    b = h->current_block;

    if (!b) {
        if (h->rank == h->rank_master) {
            fprintf(stderr, "*** ERROR ***\n");
            fprintf(stderr, "SDF block not initialised. Ignoring call.\n");
        }
        return 1;
    }

    if (b->done_header) {
        h->current_location = b->block_start + h->block_header_length;
        return 0;
    }

    sdf_indent = 2;
    SDF_DPRNT("\n");

    h->current_location = b->block_start;

    SDF_READ_ENTRY_INT8(b->next_block_location);

    SDF_READ_ENTRY_INT8(b->data_location);

    SDF_READ_ENTRY_ID(b->id);

    SDF_READ_ENTRY_INT8(b->data_length);

    SDF_READ_ENTRY_TYPE(blocktype);

    SDF_READ_ENTRY_TYPE(datatype);

    SDF_READ_ENTRY_INT4(b->ndims);

    SDF_READ_ENTRY_STRING(b->name);

    b->stagger = SDF_STAGGER_CELL_CENTRE;
    for (i = 0; i < 3; i++) b->dims[i] = 1;

    b->done_header = 1;
    h->current_location = b->block_start + h->block_header_length;

    switch (b->datatype) {
    case(SDF_DATATYPE_REAL4):
        b->type_size = 4;
        break;
    case(SDF_DATATYPE_REAL8):
        b->type_size = 8;
        break;
    case(SDF_DATATYPE_INTEGER4):
        b->type_size = 4;
        break;
    case(SDF_DATATYPE_INTEGER8):
        b->type_size = 8;
        break;
    case(SDF_DATATYPE_CHARACTER):
        b->type_size = 1;
        break;
    case(SDF_DATATYPE_LOGICAL):
        b->type_size = 1;
        break;
    }
    b->datatype_out = b->datatype;
    b->type_size_out = b->type_size;
#ifdef PARALLEL
    switch (b->datatype) {
    case(SDF_DATATYPE_REAL4):
        b->mpitype = MPI_FLOAT;
        break;
    case(SDF_DATATYPE_REAL8):
        b->mpitype = MPI_DOUBLE;
        break;
    case(SDF_DATATYPE_INTEGER4):
        b->mpitype = MPI_INT;
        break;
    case(SDF_DATATYPE_INTEGER8):
        b->mpitype = MPI_LONG_LONG;
        break;
    case(SDF_DATATYPE_CHARACTER):
        b->mpitype = MPI_CHAR;
        break;
    case(SDF_DATATYPE_LOGICAL):
        b->mpitype = MPI_CHAR;
        break;
    }
    b->mpitype_out = b->mpitype;
#endif

    return 0;
}


int sdf_read_block_info(sdf_file_t *h)
{
    sdf_block_t *b;
    int ret;

    sdf_read_next_block_header(h);
    b = h->current_block;
    if (b->done_info) return 0;

    sdf_indent += 2;
    if (b->blocktype == SDF_BLOCKTYPE_PLAIN_MESH)
        ret = sdf_read_plain_mesh_info(h);
    else if (b->blocktype == SDF_BLOCKTYPE_POINT_MESH)
        ret = sdf_read_point_mesh_info(h);
    else if (b->blocktype == SDF_BLOCKTYPE_PLAIN_VARIABLE)
        ret = sdf_read_variable_info(h);
    else if (b->blocktype == SDF_BLOCKTYPE_POINT_VARIABLE)
        ret = sdf_read_point_variable_info(h);
    else if (b->blocktype == SDF_BLOCKTYPE_CONSTANT)
        ret = sdf_read_constant(h);
/*
    else if (b->blocktype == SDF_BLOCKTYPE_ARRAY)
        ret = sdf_read_array_info(h);
    else if (b->blocktype == SDF_BLOCKTYPE_RUN_INFO)
        ret = sdf_read_run_info(h);
*/
    else if (b->blocktype == SDF_BLOCKTYPE_STITCHED_TENSOR)
        ret = sdf_read_stitched_tensor(h);
    else if (b->blocktype == SDF_BLOCKTYPE_STITCHED_MATERIAL)
        ret = sdf_read_stitched_material(h);
    else if (b->blocktype == SDF_BLOCKTYPE_STITCHED_MATVAR)
        ret = sdf_read_stitched_matvar(h);
    else if (b->blocktype == SDF_BLOCKTYPE_STITCHED_SPECIES)
        ret = sdf_read_stitched_species(h);

    return ret;
}



int sdf_read_blocklist(sdf_file_t *h)
{
    int i, buflen;

    if (h->blocklist) {
        h->current_block = h->blocklist;
        return 0;
    }

    if (!h->done_header) sdf_read_header(h);
    h->current_block = NULL;

    // Read the whole summary block into a temporary buffer on rank 0
    buflen = h->summary_size;
    h->current_location = h->start_location = h->summary_location;
    h->buffer = malloc(buflen);

    if (h->rank == h->rank_master) {
        sdf_seek(h);
        sdf_read_bytes(h, h->buffer, buflen);
    }

    // Send the temporary buffer to all processors
    sdf_broadcast(h, h->buffer, buflen);

    // Construct the metadata blocklist using the contents of the buffer
    for (i = 0; i < h->nblocks; i++) {
        sdf_read_block_info(h);
    }

    free(h->buffer);
    h->buffer = NULL;
    h->current_block = h->blocklist;

    return 0;
}

#define SDF_COMMON_INFO() do { \
    if (!h->current_block || !h->current_block->done_header) { \
      if (h->rank == h->rank_master) { \
        fprintf(stderr, "*** ERROR ***\n"); \
        fprintf(stderr, "SDF block header has not been read." \
                " Ignoring call.\n"); \
      } \
      return 1; \
    } \
    b = h->current_block; \
    if (b->done_info) return 0; \
    h->current_location = b->block_start + h->block_header_length; \
    b->done_info = 1; } while(0)



int sdf_read_stitched_tensor(sdf_file_t *h)
{
    sdf_block_t *b;
    int i;

    SDF_COMMON_INFO();

    // Metadata is
    // - stagger   INTEGER(i4)
    // - meshid    CHARACTER(id_length)
    // - varids    ndims*CHARACTER(id_length)

    SDF_READ_ENTRY_INT4(b->stagger);

    SDF_READ_ENTRY_ID(b->mesh_id);

    SDF_READ_ENTRY_ARRAY_ID(b->variable_ids, b->ndims);

    b->done_data = 1;

    return 0;
}



int sdf_read_stitched_material(sdf_file_t *h)
{
    sdf_block_t *b;
    int i;

    SDF_COMMON_INFO();

    // Metadata is
    // - stagger   INTEGER(i4)
    // - meshid    CHARACTER(id_length)
    // - matnames  ndims*CHARACTER(string_length)
    // - varids    ndims*CHARACTER(id_length)

    SDF_READ_ENTRY_INT4(b->stagger);

    SDF_READ_ENTRY_ID(b->mesh_id);

    SDF_READ_ENTRY_ARRAY_STRING(b->material_names, b->ndims);

    SDF_READ_ENTRY_ARRAY_ID(b->variable_ids, b->ndims);

    b->done_data = 1;

    return 0;
}



int sdf_read_stitched_matvar(sdf_file_t *h)
{
    sdf_block_t *b;
    int i;

    SDF_COMMON_INFO();

    // Metadata is
    // - stagger   INTEGER(i4)
    // - meshid    CHARACTER(id_length)
    // - matid     CHARACTER(id_length)
    // - varids    ndims*CHARACTER(id_length)

    SDF_READ_ENTRY_INT4(b->stagger);

    SDF_READ_ENTRY_ID(b->mesh_id);

    SDF_READ_ENTRY_ID(b->material_id);

    SDF_READ_ENTRY_ARRAY_ID(b->variable_ids, b->ndims);

    b->done_data = 1;

    return 0;
}



int sdf_read_stitched_species(sdf_file_t *h)
{
    sdf_block_t *b;
    int i;

    SDF_COMMON_INFO();

    // Metadata is
    // - stagger   INTEGER(i4)
    // - meshid    CHARACTER(id_length)
    // - matid     CHARACTER(id_length)
    // - matname   CHARACTER(string_length)
    // - specnames ndims*CHARACTER(string_length)
    // - varids    ndims*CHARACTER(id_length)

    SDF_READ_ENTRY_INT4(b->stagger);

    SDF_READ_ENTRY_ID(b->mesh_id);

    SDF_READ_ENTRY_ID(b->material_id);

    SDF_READ_ENTRY_STRING(b->material_name);

    SDF_READ_ENTRY_ARRAY_STRING(b->material_names, b->ndims);

    SDF_READ_ENTRY_ARRAY_ID(b->variable_ids, b->ndims);

    b->done_data = 1;

    return 0;
}



int sdf_read_constant(sdf_file_t *h)
{
    sdf_block_t *b;

    // Metadata is
    // - value     TYPE_SIZE

    b = h->current_block;
    SDF_READ_ENTRY_CONST(b->const_value);

    b->stagger = SDF_STAGGER_VERTEX;
    h->current_location = b->block_start + b->info_length;

    return 0;
}
