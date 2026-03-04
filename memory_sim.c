#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "1.1.0"

#define PHYSICAL_MEMORY_SIZE 1024
#define PAGE_SIZE 64
#define NUM_FRAMES (PHYSICAL_MEMORY_SIZE / PAGE_SIZE)

#define VIRTUAL_ADDRESS_SPACE_SIZE 4096
#define NUM_VIRTUAL_PAGES (VIRTUAL_ADDRESS_SPACE_SIZE / PAGE_SIZE)

#define SEGMENT_TABLE_SIZE 10

typedef struct {
    int page_number;
    int frame_number;
    bool valid;
    bool dirty;
    int last_access;
} PageTableEntry;

typedef struct {
    int base;
    int size;
    bool allocated;
    int process_id;
} SegmentTableEntry;

typedef struct {
    char data[PHYSICAL_MEMORY_SIZE];
    bool frames_used[NUM_FRAMES];
    int access_counter;
} PhysicalMemory;

typedef struct {
    unsigned int page_faults;
    unsigned int page_hits;
    unsigned int page_replacements;
    unsigned int disk_writes;
    unsigned int read_ops;
    unsigned int write_ops;
    unsigned int segment_allocations;
    unsigned int segment_alloc_failures;
} MemoryStats;

typedef struct {
    PageTableEntry page_table[NUM_VIRTUAL_PAGES];
    SegmentTableEntry segment_table[SEGMENT_TABLE_SIZE];
    int num_resident_pages;
    int num_segments;
    MemoryStats stats;
} MemoryManager;

static bool is_valid_page_number(int page_num) {
    return page_num >= 0 && page_num < NUM_VIRTUAL_PAGES;
}

static bool is_valid_offset(int offset) {
    return offset >= 0 && offset < PAGE_SIZE;
}

static void init_memory_manager(MemoryManager *mm, PhysicalMemory *pm) {
    mm->num_resident_pages = 0;
    mm->num_segments = 0;
    memset(&mm->stats, 0, sizeof(mm->stats));

    pm->access_counter = 0;
    memset(pm->data, 0, sizeof(pm->data));

    for (int i = 0; i < NUM_VIRTUAL_PAGES; i++) {
        mm->page_table[i].page_number = i;
        mm->page_table[i].frame_number = -1;
        mm->page_table[i].valid = false;
        mm->page_table[i].dirty = false;
        mm->page_table[i].last_access = 0;
    }

    for (int i = 0; i < NUM_FRAMES; i++) {
        pm->frames_used[i] = false;
    }

    for (int i = 0; i < SEGMENT_TABLE_SIZE; i++) {
        mm->segment_table[i].base = 0;
        mm->segment_table[i].size = 0;
        mm->segment_table[i].allocated = false;
        mm->segment_table[i].process_id = 0;
    }
}

static int find_free_frame(const PhysicalMemory *pm) {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (!pm->frames_used[i]) {
            return i;
        }
    }
    return -1;
}

static int select_lru_victim_page(const MemoryManager *mm) {
    int victim_page = -1;
    int min_access = INT_MAX;

    for (int i = 0; i < NUM_VIRTUAL_PAGES; i++) {
        const PageTableEntry *entry = &mm->page_table[i];
        if (entry->valid && entry->last_access < min_access) {
            min_access = entry->last_access;
            victim_page = i;
        }
    }

    return victim_page;
}

static int evict_page(MemoryManager *mm, int victim_page) {
    if (!is_valid_page_number(victim_page) || !mm->page_table[victim_page].valid) {
        return -1;
    }

    PageTableEntry *victim = &mm->page_table[victim_page];
    int frame = victim->frame_number;

    if (victim->dirty) {
        mm->stats.disk_writes++;
    }

    victim->valid = false;
    victim->dirty = false;
    victim->frame_number = -1;
    victim->last_access = 0;

    mm->num_resident_pages--;

    return frame;
}

static int allocate_page(MemoryManager *mm, PhysicalMemory *pm, int page_num) {
    if (!is_valid_page_number(page_num)) {
        fprintf(stderr, "Error: invalid page number %d\n", page_num);
        return -1;
    }

    PageTableEntry *entry = &mm->page_table[page_num];
    if (entry->valid) {
        return entry->frame_number;
    }

    mm->stats.page_faults++;

    int frame = find_free_frame(pm);
    if (frame == -1) {
        int victim_page = select_lru_victim_page(mm);
        if (victim_page == -1) {
            fprintf(stderr, "Error: unable to find LRU victim page\n");
            return -1;
        }

        frame = evict_page(mm, victim_page);
        if (frame < 0) {
            fprintf(stderr, "Error: failed to evict page %d\n", victim_page);
            return -1;
        }

        mm->stats.page_replacements++;
    }

    entry->frame_number = frame;
    entry->valid = true;
    entry->dirty = false;
    entry->last_access = pm->access_counter++;

    pm->frames_used[frame] = true;
    mm->num_resident_pages++;

    memset(&pm->data[frame * PAGE_SIZE], 0, PAGE_SIZE);

    return frame;
}

static int ensure_page_present(MemoryManager *mm, PhysicalMemory *pm, int page_num) {
    if (!is_valid_page_number(page_num)) {
        fprintf(stderr, "Error: invalid page number %d\n", page_num);
        return -1;
    }

    PageTableEntry *entry = &mm->page_table[page_num];
    if (!entry->valid) {
        return allocate_page(mm, pm, page_num);
    }

    mm->stats.page_hits++;
    entry->last_access = pm->access_counter++;
    return entry->frame_number;
}

static int read_byte(MemoryManager *mm, PhysicalMemory *pm, int page_num, int offset, char *out_value) {
    if (out_value == NULL) {
        fprintf(stderr, "Error: null output pointer\n");
        return -1;
    }

    if (!is_valid_offset(offset)) {
        fprintf(stderr, "Error: invalid offset %d\n", offset);
        return -1;
    }

    int frame = ensure_page_present(mm, pm, page_num);
    if (frame < 0) {
        return -1;
    }

    int physical_address = frame * PAGE_SIZE + offset;
    *out_value = pm->data[physical_address];
    mm->stats.read_ops++;
    return 0;
}

static int write_byte(MemoryManager *mm, PhysicalMemory *pm, int page_num, int offset, char value) {
    if (!is_valid_offset(offset)) {
        fprintf(stderr, "Error: invalid offset %d\n", offset);
        return -1;
    }

    int frame = ensure_page_present(mm, pm, page_num);
    if (frame < 0) {
        return -1;
    }

    int physical_address = frame * PAGE_SIZE + offset;
    pm->data[physical_address] = value;

    mm->page_table[page_num].dirty = true;
    mm->page_table[page_num].last_access = pm->access_counter++;
    mm->stats.write_ops++;

    return 0;
}

static int free_page(MemoryManager *mm, PhysicalMemory *pm, int page_num) {
    if (!is_valid_page_number(page_num)) {
        fprintf(stderr, "Error: invalid page number %d\n", page_num);
        return -1;
    }

    PageTableEntry *entry = &mm->page_table[page_num];
    if (!entry->valid) {
        fprintf(stderr, "Warning: page %d is not allocated\n", page_num);
        return -1;
    }

    int frame = entry->frame_number;

    pm->frames_used[frame] = false;
    memset(&pm->data[frame * PAGE_SIZE], 0, PAGE_SIZE);

    entry->valid = false;
    entry->dirty = false;
    entry->frame_number = -1;
    entry->last_access = 0;

    mm->num_resident_pages--;
    return 0;
}

static int find_free_segment_slot(const MemoryManager *mm) {
    for (int i = 0; i < SEGMENT_TABLE_SIZE; i++) {
        if (!mm->segment_table[i].allocated) {
            return i;
        }
    }
    return -1;
}

static void sort_segment_indices_by_base(const MemoryManager *mm, int *indices, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            int base_i = mm->segment_table[indices[i]].base;
            int base_j = mm->segment_table[indices[j]].base;
            if (base_i > base_j) {
                int tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }
}

static int find_segment_base_first_fit(const MemoryManager *mm, int size) {
    if (size <= 0 || size > VIRTUAL_ADDRESS_SPACE_SIZE) {
        return -1;
    }

    int allocated_indices[SEGMENT_TABLE_SIZE];
    int allocated_count = 0;

    for (int i = 0; i < SEGMENT_TABLE_SIZE; i++) {
        if (mm->segment_table[i].allocated) {
            allocated_indices[allocated_count++] = i;
        }
    }

    if (allocated_count == 0) {
        return 0;
    }

    sort_segment_indices_by_base(mm, allocated_indices, allocated_count);

    int cursor = 0;
    for (int i = 0; i < allocated_count; i++) {
        const SegmentTableEntry *segment = &mm->segment_table[allocated_indices[i]];
        if (cursor + size <= segment->base) {
            return cursor;
        }
        cursor = segment->base + segment->size;
    }

    if (cursor + size <= VIRTUAL_ADDRESS_SPACE_SIZE) {
        return cursor;
    }

    return -1;
}

static int allocate_segment(MemoryManager *mm, int size, int process_id) {
    if (size <= 0) {
        fprintf(stderr, "Error: segment size must be > 0\n");
        mm->stats.segment_alloc_failures++;
        return -1;
    }

    int slot = find_free_segment_slot(mm);
    if (slot < 0) {
        fprintf(stderr, "Error: segment table is full\n");
        mm->stats.segment_alloc_failures++;
        return -1;
    }

    int base = find_segment_base_first_fit(mm, size);
    if (base < 0) {
        fprintf(stderr, "Error: no contiguous virtual space for segment size %d\n", size);
        mm->stats.segment_alloc_failures++;
        return -1;
    }

    mm->segment_table[slot].base = base;
    mm->segment_table[slot].size = size;
    mm->segment_table[slot].allocated = true;
    mm->segment_table[slot].process_id = process_id;

    mm->num_segments++;
    mm->stats.segment_allocations++;

    return slot;
}

static int free_segment(MemoryManager *mm, int segment_id) {
    if (segment_id < 0 || segment_id >= SEGMENT_TABLE_SIZE) {
        fprintf(stderr, "Error: invalid segment id %d\n", segment_id);
        return -1;
    }

    SegmentTableEntry *entry = &mm->segment_table[segment_id];
    if (!entry->allocated) {
        fprintf(stderr, "Warning: segment %d is not allocated\n", segment_id);
        return -1;
    }

    entry->base = 0;
    entry->size = 0;
    entry->allocated = false;
    entry->process_id = 0;

    mm->num_segments--;

    return 0;
}

static int get_physical_address(const MemoryManager *mm, int page_num, int offset) {
    if (!is_valid_page_number(page_num) || !is_valid_offset(offset)) {
        return -1;
    }

    const PageTableEntry *entry = &mm->page_table[page_num];
    if (!entry->valid) {
        return -1;
    }

    return entry->frame_number * PAGE_SIZE + offset;
}

static void print_config(void) {
    printf("Memory Manager Simulator v%s\n", VERSION);
    printf("----------------------------------------\n");
    printf("Physical memory size : %d bytes\n", PHYSICAL_MEMORY_SIZE);
    printf("Page size            : %d bytes\n", PAGE_SIZE);
    printf("Physical frames      : %d\n", NUM_FRAMES);
    printf("Virtual pages        : %d\n", NUM_VIRTUAL_PAGES);
    printf("Segment slots        : %d\n", SEGMENT_TABLE_SIZE);
    printf("\n");
}

static void print_page_table(const MemoryManager *mm) {
    printf("Page Table (resident pages)\n");
    printf("Page | Frame | V | D | LastAccess\n");
    printf("-----+-------+---+---+-----------\n");

    int resident_count = 0;
    for (int i = 0; i < NUM_VIRTUAL_PAGES; i++) {
        const PageTableEntry *entry = &mm->page_table[i];
        if (entry->valid) {
            printf("%4d | %5d | %c | %c | %10d\n",
                   i,
                   entry->frame_number,
                   entry->valid ? 'Y' : 'N',
                   entry->dirty ? 'Y' : 'N',
                   entry->last_access);
            resident_count++;
        }
    }

    if (resident_count == 0) {
        printf("(no resident pages)\n");
    }

    printf("Total resident pages: %d/%d\n\n", mm->num_resident_pages, NUM_FRAMES);
}

static void print_segment_table(const MemoryManager *mm) {
    printf("Segment Table\n");
    printf("Seg | Base | Size | End  | PID\n");
    printf("----+------+------+------+-----\n");

    int allocated_count = 0;
    for (int i = 0; i < SEGMENT_TABLE_SIZE; i++) {
        const SegmentTableEntry *entry = &mm->segment_table[i];
        if (entry->allocated) {
            int end = entry->base + entry->size;
            printf("%3d | %4d | %4d | %4d | %3d\n",
                   i,
                   entry->base,
                   entry->size,
                   end,
                   entry->process_id);
            allocated_count++;
        }
    }

    if (allocated_count == 0) {
        printf("(no allocated segments)\n");
    }

    printf("Total segments: %d/%d\n\n", mm->num_segments, SEGMENT_TABLE_SIZE);
}

static void print_memory_usage(const PhysicalMemory *pm) {
    int used_frames = 0;
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (pm->frames_used[i]) {
            used_frames++;
        }
    }

    double usage_percent = (double)used_frames * 100.0 / (double)NUM_FRAMES;

    printf("Physical Memory Usage\n");
    printf("Used frames: %d/%d (%.1f%%)\n", used_frames, NUM_FRAMES, usage_percent);
    printf("Bytes used : %d/%d\n", used_frames * PAGE_SIZE, PHYSICAL_MEMORY_SIZE);

    printf("Frames map : [");
    for (int i = 0; i < NUM_FRAMES; i++) {
        putchar(pm->frames_used[i] ? '#' : '.');
    }
    printf("]\n\n");
}

static void print_stats(const MemoryManager *mm) {
    const MemoryStats *stats = &mm->stats;

    printf("Runtime Stats\n");
    printf("Page faults         : %u\n", stats->page_faults);
    printf("Page hits           : %u\n", stats->page_hits);
    printf("Page replacements   : %u\n", stats->page_replacements);
    printf("Disk writes (dirty) : %u\n", stats->disk_writes);
    printf("Read operations     : %u\n", stats->read_ops);
    printf("Write operations    : %u\n", stats->write_ops);
    printf("Segment allocations : %u\n", stats->segment_allocations);
    printf("Segment alloc fails : %u\n", stats->segment_alloc_failures);

    unsigned int total_accesses = stats->page_hits + stats->page_faults;
    if (total_accesses > 0) {
        double hit_rate = (double)stats->page_hits * 100.0 / (double)total_accesses;
        printf("Hit rate            : %.2f%%\n", hit_rate);
    }

    printf("\n");
}

static void run_simulation(void) {
    MemoryManager mm;
    PhysicalMemory pm;

    init_memory_manager(&mm, &pm);
    print_config();

    printf("Scenario 1: paging + LRU replacement\n");
    printf("-------------------------------------\n");

    for (int page = 0; page < NUM_FRAMES + 6; page++) {
        char value = (char)('A' + (page % 26));
        if (write_byte(&mm, &pm, page, 0, value) != 0) {
            fprintf(stderr, "Warning: write failed for page %d\n", page);
        }
    }

    int hot_pages[] = {4, 7, 12, 18, 20};
    int hot_count = (int)(sizeof(hot_pages) / sizeof(hot_pages[0]));
    for (int i = 0; i < hot_count; i++) {
        char value = '\0';
        if (read_byte(&mm, &pm, hot_pages[i], 0, &value) != 0) {
            fprintf(stderr, "Warning: read failed for page %d\n", hot_pages[i]);
        }
    }

    free_page(&mm, &pm, 7);

    char sample = '\0';
    if (read_byte(&mm, &pm, 20, 0, &sample) == 0) {
        int physical_address = get_physical_address(&mm, 20, 0);
        printf("Address translation example: page 20, offset 0 -> PA %d, value %c\n\n",
               physical_address,
               sample);
    }

    print_page_table(&mm);
    print_memory_usage(&pm);

    printf("Scenario 2: segmentation with first-fit\n");
    printf("----------------------------------------\n");

    int seg_a = allocate_segment(&mm, 300, 1001);
    int seg_b = allocate_segment(&mm, 600, 1002);
    int seg_c = allocate_segment(&mm, 200, 1003);

    if (seg_a >= 0 && seg_b >= 0 && seg_c >= 0) {
        free_segment(&mm, seg_b);
        allocate_segment(&mm, 120, 2001);
    }

    print_segment_table(&mm);
    print_stats(&mm);
}

static void print_help(const char *program_name) {
    printf("Usage: %s [--help] [--version]\n", program_name);
    printf("\n");
    printf("If no argument is provided, the default simulation is executed.\n");
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_help(argv[0]);
            return 0;
        }

        if (strcmp(argv[1], "--version") == 0) {
            printf("%s\n", VERSION);
            return 0;
        }

        fprintf(stderr, "Unknown argument: %s\n", argv[1]);
        print_help(argv[0]);
        return 1;
    }

    run_simulation();
    return 0;
}
