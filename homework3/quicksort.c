/*********************************************/
/**    OS Lab homeworks 2018 repository     **/
/**         UNICT - Lorenzo Catania         **/
/**      Parts of this code may be from     **/
/**   Prof. Mario Di Raimondo's solutions   **/
/*********************************************/

char *search_pivot(char *map, int i, int j, int size) {
    int k;

    for (k=i+1; k<=j; k++) {
        if (memcmp(map+size*k, map+size*i, size) > 0) // map[k] > map[i]
            return(memcpy(malloc(size), map+size*k, size));     // pivot = map[k]
        else if (memcmp(map+size*k, map+size*i, size) < 0) // map[k] < map[i]
            return(memcpy(malloc(size), map+size*i, size));    // pivot = map[i]
    }
    return(NULL);
}

// partiziona map[p]...map[r] usando il pivot
int partition(char *map, int p, int r, char *pivot, int size, char* swap_buffer) {
    int i, j;

    i = p;
    j = r;
    do {
        while (memcmp(map+size*j, pivot, size) >= 0) // map[j] >= pivot
            j--;
        while (memcmp(map+size*i, pivot, size) < 0)    // map[i] < pivot
            i++;
        if (i<j) { // map[i] <-> map[j]
            memcpy(swap_buffer, map+size*i, size);
            memcpy(map+size*i, map+size*j, size);
            memcpy(map+size*j, swap_buffer, size);
        }
    } while (i<j);
    return(j);
}

// quicksort in versione ricorsiva
void quicksort(char *map, int p, int r, int size, char* swap_buffer) {
    int q;
    char *pivot;

    pivot = search_pivot(map, p, r, size);
    if ( (p < r) && (pivot != NULL) ) {
        q = partition(map, p, r, pivot, size, swap_buffer);
        quicksort(map, p, q, size, swap_buffer);
        quicksort(map, q+1, r, size, swap_buffer);
    }
    if (pivot) free(pivot);
}