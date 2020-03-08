// Aiko is (c) Andy Gelme and Pete Yandell https://github.com/geekscape/aiko_arduino_legacy

#include "AikoEvents.h"

#include <stdlib.h>
#include "Arduino.h"

namespace Aiko {

  /* EventHanderList */

  void EventHandlerList::add(EventHandler* handler) {
    if (firstHandler_)
      last()->next_ = handler;
    else
      firstHandler_ = handler;
    if (!nextHandler_) nextHandler_ = handler;
    handler->next_ = 0;
  }

  void EventHandlerList::flush() {
    firstHandler_ = 0;
    nextHandler_  = 0;
  }

  EventHandler* EventHandlerList::handlerBefore(EventHandler* handler) {
    EventHandler* previousHandler = firstHandler_;
    while (previousHandler && previousHandler->next_ != handler) previousHandler = previousHandler->next_;
    return previousHandler;
  }

  EventHandler* EventHandlerList::last() {
    EventHandler* lastHandler = firstHandler_;
    while (lastHandler->next_) lastHandler = lastHandler->next_;
    return lastHandler;
  }
  
  EventHandler* EventHandlerList::next() {
    EventHandler* handler = nextHandler_;
    if (handler) nextHandler_ = handler->next_;
    return handler;
  }

  void EventHandlerList::remove(EventHandler* handler) {
    if (handler == firstHandler_)
      firstHandler_ = handler->next_;
    else
      handlerBefore(handler)->next_ = handler->next_;
    if (handler == nextHandler_) nextHandler_ = handler->next_;
    handler->next_ = 0;
  }

  void EventHandlerList::resetIterator() {
    nextHandler_ = firstHandler_;
  }


  /* EventManager */

  EventManager Events;
 
  void EventManager::addHandler(EventHandler* handler) {
    handlerList_.add(handler);
  }

  // allow 2 queues: one that is ISR safe, and one that is not. Say which queue to add the job, in.
  void EventManager::addHandler(void (*handlerFunction)(), unsigned int period, bool in_ISR, unsigned int delay) {
    EventHandler* handler = static_cast<EventHandler*>(malloc(sizeof(EventHandler)));
    handler->callback_  = functionCallback(handlerFunction);
    handler->in_ISR     = in_ISR;
    handler->period_    = period;
    handler->countdown_ = delay;
    addHandler(handler);
  }

  void EventManager::addOneShotHandler(void (*handlerFunction)(), unsigned int delay) {
    addHandler(handlerFunction, 0, delay);
  }

  // Specify whether to run the default queue, or the ISR safe queue
  void EventManager::loop(bool run_ISR, unsigned long time) {
    if (!isRunning_) start(time);
    long elapsed = time - lastLoopTime_;

    handlerList_.resetIterator();
    while (EventHandler* handler = handlerList_.next()) {
      if (handler->in_ISR != run_ISR) continue;
      handler->countdown_ -= elapsed;
    }
      
    handlerList_.resetIterator();
    while (EventHandler* handler = handlerList_.next()) {
      if (handler->in_ISR != run_ISR) continue;
      if (handler->countdown_ <= 0) {
        handler->fire();
        if (handler->period_ > 0)
          handler->countdown_ += handler->period_;
        else {
          removeHandler(handler);
          free(handler);
        }
      }
    }

    lastLoopTime_ = time;
  }

  void EventManager::removeHandler(EventHandler* handler) {
    handlerList_.remove(handler);
  }

  void EventManager::reset() {
    handlerList_.flush();
    isRunning_ = false;
  }

  void EventManager::start(unsigned long time) {
    lastLoopTime_ = time;
    isRunning_ = true;
  }
  
};
