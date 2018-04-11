#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    //std::cout << "Store" << __PRETTY_FUNCTION__ << std::endl;
    // set beginning and end of routine's stack
    char stack_end;
    ctx.Low = StackBottom;
    ctx.Hight = &stack_end;

    // copy routine's stack in context's buffer

    size_t delta = ctx.Low - ctx.Hight;// because stack grows downwards

    if (std::get<0>(ctx.Stack) != nullptr) {
      delete[] std::get<0>(ctx.Stack);
    }
    std::get<1>(ctx.Stack) = delta;
    std::get<0>(ctx.Stack) = new char[delta]; // allocate memory for copy of stack
    // create full copy of stack
    // note that stack grows downwards, so we take these addresses as start points for copying
    memcpy(std::get<0>(ctx.Stack), ctx.Hight, delta);
}

void Engine::Restore(context &ctx) {
//    std::cout << "coroutine debug: " << __PRETTY_FUNCTION__ << std::endl;
    // write stack copy from context into actual stack
    char stack_end;
    // we may have to enlarge stack so that we could paste our copy
    if (&stack_end > StackBottom - std::get<1>(ctx.Stack) ) {
        Restore(ctx);
    }

    memcpy(ctx.Hight, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
    longjmp(ctx.Environment, 1);  // val is what is returned by setjmp
}

void Engine::yield() {
//    std::cout << "coroutine debug: " << __PRETTY_FUNCTION__ << std::endl;
    if (alive != nullptr) {
        // take the first task from alive list
        context *new_routine = alive;
        if (alive->next != nullptr) {
          alive->next->prev = nullptr;
        }
        alive = alive->next;
        sched(new_routine);
    }
}

void Engine::sched(void *routine_) {
//    std::cout << "coroutine debug: " << __PRETTY_FUNCTION__ << std::endl;
    context *ctx = (context*)routine_;
    //Функция setjmp() сохраняет содержимое стека системы в буфере envbuf для
    //дальнейшего ис­пользования функцией longjmp().
    if (cur_routine != nullptr ){
        Store(*cur_routine);
        if (setjmp(cur_routine->Environment) > 0) {
          return;
        }
    }
    cur_routine = ctx;
    Restore(*ctx);
}

} // namespace Coroutine
} // namespace Afina
