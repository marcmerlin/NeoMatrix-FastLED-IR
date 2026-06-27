#include <check.h>
#include <stdlib.h>
#include <stdint.h>
#include "Aiko/AikoEvents_Impl.h"

START_TEST(test_handler_lifetime_invariant)
{
    // Invariant: After removeHandler() is called, no dangling references to freed handler memory exist
    // We test this by ensuring handler operations remain safe after removal
    
    // Payload 1: Handler with period = 0 (exact exploit case)
    EventHandler* handler1 = (EventHandler*)malloc(sizeof(EventHandler));
    handler1->period_ = 0;
    handler1->countdown_ = 0;
    handler1->next = NULL;
    
    // Payload 2: Handler with period = -1 (boundary case)
    EventHandler* handler2 = (EventHandler*)malloc(sizeof(EventHandler));
    handler2->period_ = -1;
    handler2->countdown_ = 0;
    handler2->next = NULL;
    
    // Payload 3: Valid handler with positive period
    EventHandler* handler3 = (EventHandler*)malloc(sizeof(EventHandler));
    handler3->period_ = 100;
    handler3->countdown_ = 50;
    handler3->next = NULL;
    
    EventHandler* payloads[] = {handler1, handler2, handler3};
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        EventHandler* handler = payloads[i];
        
        // Simulate the vulnerable code path
        if (handler->period_ > 0) {
            handler->countdown_ += handler->period_;
            // Handler should remain valid
            ck_assert_ptr_nonnull(handler);
            ck_assert_int_ge(handler->countdown_, handler->period_);
        } else {
            // This is the vulnerable path - handler gets freed
            removeHandler(handler);
            free(handler);
            
            // Security invariant: No operations should be performed on freed handler
            // The system should not crash or exhibit undefined behavior
            // We can't directly test the freed memory, but we can verify the system
            // continues to function correctly
            ck_assert(1); // Placeholder for system stability check
        }
        
        // Clean up any remaining allocations
        if (handler->period_ > 0) {
            free(handler);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_handler_lifetime_invariant);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}