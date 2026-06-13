#ifndef SHARED_PAGER_H
#define SHARED_PAGER_H

#include <Arduino.h>

struct Pager {
    uint8_t totalItems;
    uint8_t itemsPerPage;
    uint8_t currentPage;
    uint8_t selectedIdx;

    uint8_t pageStart() const { return currentPage * itemsPerPage; }
    uint8_t pageEnd() const {
        uint8_t e = pageStart() + itemsPerPage;
        return (e > totalItems) ? totalItems : e;
    }
    bool hasNext() const { return pageEnd() < totalItems; }
    bool hasPrev() const { return currentPage > 0; }
    void nextPage() { if (hasNext()) currentPage++; }
    void prevPage() { if (hasPrev()) currentPage--; }
    void reset() { currentPage = 0; selectedIdx = 0; }

    void moveDown() {
        if (selectedIdx < totalItems - 1) {
            selectedIdx++;
            if (selectedIdx >= pageEnd()) {
                currentPage++;
            }
        }
    }
    void moveUp() {
        if (selectedIdx > 0) {
            selectedIdx--;
            if (selectedIdx < pageStart()) {
                currentPage--;
            }
        }
    }
};

#endif
