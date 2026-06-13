#ifndef NAV_STACK_H
#define NAV_STACK_H

#include "app_types.h"

#define NAV_STACK_DEPTH 8

struct NavStack {
    Page history[NAV_STACK_DEPTH];
    int8_t top = -1;

    void push(Page screen) {
        if (top < NAV_STACK_DEPTH - 1) {
            history[++top] = screen;
        }
    }
    Page pop() {
        if (top >= 0) {
            return history[top--];
        }
        return Page::Main; // Default to main menu
    }
    void clear() {
        top = -1;
    }
    bool isEmpty() const {
        return top == -1;
    }
};

extern NavStack navStack;

#endif
