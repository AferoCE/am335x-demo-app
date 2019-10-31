/**
   Copyright 2019 Afero, Inc.
   This program is a demonstration program that shows you how to capture
   an event from the Afero Cloud to set an edge attribute, and do something with 
   the data that is sent to you. It also inludes the ability to send data
   items from the edge device up to the Cloud. This basic functionality is
   really the basis of all communications with the Afero Cloud and the device.
   Receiving an attribute is typicaly used for some kind of control of
   the device, while sending an attribute to the Cloud is typically used
   for reporting some kind of operational status from the device back to the
   Cloud and mobile app.

   You may wish to watch the git repository from which you pulled this sample
   code as it will become more fleshed out over time with added features and
   examples of operation for things like.. device status notifications such 
   as changes in the RSSI of wifi or blue tooth, and events as well as schedules.
*/

//
// The usual cast of characters in a typical C program.
//
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

//
// And of course, the includes that are Afero specific:
//
#include "af_attr_def.h"
#include "af_attr_client.h" // AF_ATTR_MAX_LISTEN_RANGES
#include "af_log.h"
#include "af_rpc.h"
#include "af_ipc_server.h"
#include "aflib.h"
#include "aflib_mcu.h"
//
// The following include file is probably the most important to you as the application developer.
// The file is created by the Afero Profile Editor and includes the mapping of each
// MCU attribute ID number to the Name you gave that attribute in the definition of the
// MCU attributes.
// This device-description file will change every time you edit the attributes in a Profile.
// It does not necessarily change if all you change in the Afero Profile Editor is
// the UI layout or the UI elements used.
// The file only denotes a few key items such as data sizes, data types, and attribute ID-to-name mappings.
//
#include "device-description.h" 

af_lib_t          *sAf_lib    = NULL;  // Handle used to get and set attributes in the Afero library.
struct event_base *sEventBase = NULL;  // Event base for the event handler and callback.



//
// I generally like to group some defines here for debugging purposes. Whenever I run into
// some kind of odd issue that I want to examine closely, either through logs or by creating a place
// to move a variable so that I can set a breakpoint in order to examine it, I will create an appropriate
// DEBUG_something define that let's me turn it on and off at compile time.
// You can also use an attribute to control such messaging by simply testing the contents of that
// attribute every time you decide to print a log entry. One way of doing this is to use
// an attribute of 32 bits in size and then assign bit significances in that 
// attribute for various parts of the program.
// Below are a couple examples that were used but have been since removed. Leaving them here
// as a tip/technique for debugging.
//
//#define DEBUG_ROTATES 1     // Comment out to disble logging successes.
//#define DEBUG_BIT_COUNTS 1

uint16_t  getdoubled     = 0; // Value that will get doubled, given to me by the Cloud.
uint32_t  doubled        = 0; // Value that will get pushed back to the Cloud and where the doubling is deposited.
uint32_t  getrotated     = 0; // Value that will get rotated right, given to us by the Cloud.
uint32_t  rotate         = 0; // Value to be rotated, which was given to us by the Cloud, goes here.
uint32_t  rotatedr       = 0; // Rotated right value goes here, and then gets sent to the Cloud.
uint32_t  rotatedl       = 0; // Rotated left value goes here, and then gets sent to the Cloud.
uint8_t   getadded       = 0; // Added to a running sum value. Comes from the Cloud.
uint32_t  currentsum     = 0; // Current summation value that's sent back to the Cloud. Running sum
uint8_t   readvarlog     = 0; // Used as a bool. Set by the Cloud. Response is to read var log and send last line.
uint32_t countbitsofthis = 0; // 32-bit integer from the Cloud goes here. We will count how many of the bits in the value are set to '1'.
uint8_t  numberofbits    = 0; // Result of counting bits in countbitsofthis. This gets sent to the Cloud.
unsigned char lastlineofvarlog[1536]; // Big ole string buffer to respond with.
unsigned char getreversed[1536]; // String sent to us from the Cloud. Must be reversed and sent back.
unsigned char reversed[1536];    // Reversed string that is then sent back to the Cloud.
unsigned char default_string[50]="HEY! You forgot something!"; // Replaces a null string.

//
// I need a file handle as well since one of the things I do is read a line out of /var/log/messages
// as an action in response to an attribute setting.
//

FILE *sync;  // Just a file pointer to use to rummage through /var/log/messages.


//
// This callback is executed any time ASR has information for the MCU.
// The name of this event is defined in the afLib initialization in setup().
// There are several different event types that you can receive.
// In one instance, we have an ASR notification event for things like system level changes
// where the RSSI of the Wi-Fi changes.
//
// What this demo focuses on, though, is any time an MCU attribute is sent.
// This type of event is known as an AF_LIB_EVENT_MCU_SET_REQUEST event. It is within
// that event handler that we manage all the MCU attributes that we have defined with
// the Afero Profile Editor.
//
void attrEventCallback(const af_lib_event_type_t eventType, /* The event type. */
                       const af_lib_error_t error,          /* Any error that occurred. */
                       const uint16_t attributeId,          /* The attribute ID number that is being given to us. */
                       const uint16_t valueLen,             /* The size in bytes of the data being given for that attribute. */
                       const uint8_t* value) /* And the actual value of the attribute. The value needs to be cast to its correct object.*/
  
{
    char hexBuf[80];
    bool set_succeeded = 1;
    uint8_t  ret; 
    int count=0; // I use this to keep track of things when moving character strings around.
    int index=0; // I use this to keep track of things when moving character strings around as well.
    unsigned char *where; // I use this to keep track of the string given to me by the Cloud when the attribute is a string. 
    printf("AttreibutID=%d, valueLen=%d, eventType=%d, error=%d\n",attributeId,valueLen,eventType,error);

    if( attributeId == 10 ){
      index = 5;
      index = 0;
    }
  
    AFLOG_INFO("eventType=%d, error=%d, attributeId=%d, valueLen=%d",eventType, error,attributeId,valueLen);
    
    memset(hexBuf, 0, sizeof(hexBuf)); // make sure the buffer is initialized
    if (value != NULL) {
        af_util_convert_data_to_hex_with_name("value", (uint8_t *)value, 
                                          valueLen, hexBuf, sizeof(hexBuf));
    }

    AFLOG_INFO("my-app: attrEventCallback:attrid:%d, %s, len=%d event==%d", attributeId, hexBuf, valueLen, eventType);
    switch (eventType) {
      //
      // Unsolicited notification when a non-MCU attribute changes state.
      // The event type that handles such non-MCU notifications.
      //
        case AF_LIB_EVENT_ASR_NOTIFICATION: // Non-edge attribute notify.
            AFLOG_INFO("my-app: NOTIFICATION EVENT: for attr=%d", attributeId);

            //
            // We are interested in attribute wifi rssi (65005)
	    // This is where we would see such an event.
	    // Note that we do nothing with that event except log it to /var/log/messages.
	    //
            if (attributeId == AF_ATTR_WIFISTAD_WIFI_RSSI) {
               AFLOG_INFO("my-app: EDGED: Recieved an RSSI change notification");
            }
            break;


        case AF_LIB_EVENT_ASR_SET_RESPONSE:
            AFLOG_INFO("my-app: ASR_SET_RESPONSE EVENT:");
            break;

	    //
	    // The AF_LIB_EVENT_MCU_SET_REQUEST is the event type that gets invoked when
	    // the Cloud wants to send an MCU attribute value change to the device so that
	    // the device can then "do something" with that attribute.
	    // The most efective way to parse through these is a case statement within this
	    // event handler code. This is where you would put your code to handle such events.
	    // NOTE: All of these events are handled synchronously in this example; however,
	    // in more complex environments or product functionality, it would be more likely that
	    // it would trigger an asynchronous event that, after completion, resulted in something
	    // (possibly) being sent back to the Cloud with a set attribute function call.
	    // In all cases, however, you must respond to the set event request with an af_lib_send_set_response
	    // indicating that the attribute has been succesfully received.
	    //
        case AF_LIB_EVENT_MCU_SET_REQUEST: // edge attribute set request
            AFLOG_INFO("my-app: MCU_SET_REQUEST EVENT: for attr=%d", attributeId);
            // 
            // As stated above, let's use a case statement on the attributeId that has been sent.
	    // We will use the lables that the Afero Profile Editor created for us in the form of #defines
	    // to make the code more readable. These values are defined in device-description.h file.
	    //
	    switch( attributeId ){
	      //
	      // This attribute is doubled in value, then sent back as attribute AF_DOUBLED.
	      //
	    case AF_GETDOUBLED:
	      //
	      // Create a log entry so that we know we got here. 
	      AFLOG_INFO( "my-app: SET REQUEST for attrId=AF_GETDOUBLED value was=%d",(uint32_t)*value);
	      //
	      // Go ahead and say we got the data. We need to give it a variable pointer that
	      // is usually filled with the value we got from the Cloud, but that is not necessary.
	      //
	      af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, 1, (const uint8_t *)&getdoubled);
	      //
	      // Here I'm taking the value given by the Cloud, which is declared as a uint8_t *, and I am
	      // casting it to a uint32_t * (because that's what it actually is) and then mulitplying it by 2.
	      //
	      doubled = (*(uint32_t *)value * 2);
	      
	      AFLOG_INFO( "my-app: SET REQUEST for id AF_DOUBLED to %d attempted",doubled);
	      ret = af_lib_set_attribute_32(sAf_lib, AF_DOUBLED, doubled, AF_LIB_SET_REASON_LOCAL_CHANGE);
               if (ret != AF_SUCCESS) {
                   AFLOG_ERR("my-app: af_lib_set_attribute: failed set for the test attributeId=2");
               }else
		 AFLOG_INFO("my-app: REQUEST: attrib ute id AF_DOUBLED set to %d",doubled);
	      
	      break;

	    case AF_GETROTATED:
	      AFLOG_INFO( "my-app: SET REQUEST for attrId=%d value was=%d",attributeId,*(uint32_t *)value);
	      getrotated = *(uint32_t *)value; // Secure the sent data item.
	      rotatedr = (uint32_t)getrotated >> (uint32_t)1; // Rotate the bits right by one.
	      getrotated = *(uint32_t *)value; // Secure the sent data item.
	      rotatedl = (uint32_t)getrotated << (uint32_t)1; // And to the left.
	      // Then say that we received the value.
	      af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, 1,(const uint8_t *) &getrotated);
	      //
	      // Doing a name scramble here. Cloud name in the Profile Edidtor is ROTATED,
	      //'rotate' is the copy I got from the Afero stack, and then I just 
	      //copied the value from that attribute into 'rotate'.
	      //
	      // Now I need to create two NEW attributes, but I'll need to I do two things.
	      // Mark this section with a TODO: Untie these two directions from rotated and tie
	      // to their own rotatedl and rotatedr attributes. So right now, last one in wins.
	      // NOTE: See if this results in two writes in rapid succession to the Cloud or if only the last one
	      // happens.

	      ret = af_lib_set_attribute_32(sAf_lib, AF_ROTATEDR , (uint32_t)rotatedr, AF_LIB_SET_REASON_LOCAL_CHANGE);
               if (ret != AF_SUCCESS) {
                   AFLOG_ERR("my-app: REQUEST:af_lib_set_attribute: failed set for the test attributeId=AF_ROTATEDR");
               }
	       else {
		 AFLOG_ERR("my-app: REQUEST: set attribute id AF_ROTATEDR succeeded value = %d.",rotatedr);
	       }
		 
	       ret = af_lib_set_attribute_32(sAf_lib, AF_ROTATEL , (uint32_t)rotatedl, AF_LIB_SET_REASON_LOCAL_CHANGE);
               if (ret != AF_SUCCESS) {
                   AFLOG_ERR("my-app: REQUEST:af_lib_set_attribute: failed set for the test attributeId=AG_ROTATEL");
               }
	       else {
		 AFLOG_INFO("my-app: REQUEST: set attribute id AF_ROTATEL succeeded value = %d ",rotatedl);
	       }
	      break;

	      //	    case AF_TOGGLELED:
	      //	      AFLOG_INFO( "SET REQUEST for attrId=%d value was=%d",attributeId,*value);
	      //                af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, 1, (const uint8_t *)&getadded); 
	      //	      break;

	    case AF_GETADDED:
	      AFLOG_INFO( "my-app: SET REQUEST for attrId=%d value was=%d",attributeId,*(uint8_t *)value);
	      getadded = *(uint8_t *)value; // grab the data given to us.
                af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, 1, (const uint8_t *)&getadded);
		// Okay, let's use that value.
		currentsum = currentsum + (uint32_t)getadded; // Keep it as a running summation.
		//
		//And send a copy to the Cloud!
		//
		ret = af_lib_set_attribute_32(sAf_lib, AF_CURRENTSUM, (uint32_t)currentsum, AF_LIB_SET_REASON_LOCAL_CHANGE);
               if (ret != AF_SUCCESS) {
                   AFLOG_ERR("my-app: REQUEST:af_lib_set_attribute: failed set for the test attributeId=AF_GETADDED");
               }
	       else {
		 AFLOG_INFO("my-app: REQUEST: set attribute id AF_GETADDED succeeded, set to %d.",currentsum);
	       }
	      break;

	    case AF_READVARLOG:
	      AFLOG_INFO( "my-app: SET REQUEST for attrId=READVARLOG value was=%d",*(uint8_t *)value);
	      readvarlog = *(uint8_t *)value;
	      af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, (const uint16_t)1, (const uint8_t *)&readvarlog); 
		sync = fopen("/var/log/messages", "r");
		if( sync ) { // exists! Let's rummage.
		  while( fgets(lastlineofvarlog, 1024, sync) !=NULL ) {
		    // Just search for the latest line, do nothing in the loop.
		  } 
		  AFLOG_INFO("my-app: REQUEST: Last line %s\n", lastlineofvarlog); // This is just a log... you can remove it.
		}
		index = strlen( lastlineofvarlog );
		ret = af_lib_set_attribute_str( sAf_lib, AF_LASTLINEOFVARLOG, index ,(const unsigned char *)lastlineofvarlog, AF_LIB_SET_REASON_LOCAL_CHANGE);
		if (ret != AF_SUCCESS) {
		   AFLOG_ERR("my-app: REQUEST:af_lib_set_attribute: failed set for the test attributeId=AF_LASTLINEOFVARLOG");
		  }
		  else {
		    AFLOG_INFO("my-app: set attribute id LASTLINEOFVARLOG succeeded. set to %s",lastlineofvarlog);
		  }
		  fclose(sync);

		break;

	    case AF_GETREVERSED:
	      // This will take a string that is passed in by the attribute AF_GETREVERSED
	      // and reverse the ordering of the characters in the string and then write it back
	      // to the attribute AF_REVERSED.
	      count=valueLen; // Get the length of the string we are working with.
	      //
	      // Now, if the length of the string is null, let's remind them they need to give us
	      // something to reverse! NOTE: This code is still being debugged, so the null string
	      // case is not currently handled correctly.
	      //
	      if( count == 1 && value[0] == '\0' )
		{
		  //
		  // Make sure the very first character of getreversed is null for the response to match what was sent.
		  //
		  getreversed[0]='\0'; // just like so.
		  //
		  // We need to let the Cloud know we got the string even if it was a zero-length string.
		  // Respond by saying it succeeded, the data size is one byte, and point to the null character in the string.
		  //
		  af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, 1, (const uint8_t *)getreversed);
		  //
		  // Log a Gentle reminder that we just got a null string.
		  //
		  AFLOG_INFO("my-app: Received a null string for AF_GETREVERSED. size of %d", count );
		  //
		  // Then send the string "Hey! You forgot something!" so that it's seen that the string received was null.
		  //
		  ret = af_lib_set_attribute_str( sAf_lib, AF_REVERSED, strlen(default_string) ,(const unsigned char *)default_string, AF_LIB_SET_REASON_LOCAL_CHANGE);
		  //
		  // Then log the outcome of the send.
		  //
		  if (ret != AF_SUCCESS) {
		    AFLOG_ERR("my-app: REQUEST:af_lib_set_attribute: failed set for the test attributeId=AF_GETREVERSED");
		  }
		  else {
		    AFLOG_INFO("my-app: REQUEST: set attribute id AF_REVERSED succeeded. set to %s",reversed);
		  }
		}
	      else
		
	      where = value;  // And copy the address of the string being handed to us.
	      AFLOG_INFO( "my-app: SET REQUEST for attrId=%d value was=%s",attributeId,(unsigned char *)value);
	      //
	      // Now let's get the string. Copy the number of characters we were told by
	      // the Cloud that it delivered with "valueLen". NOTE: This count does NOT include the
	      // terminating NULL for the string.
	      // 
	      while( count-- ) getreversed[index++]=*(unsigned char *)where++;
	      //
	      // Then report back to the Cloud that we have received the attribute.
	      //
              af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, 1, (const uint8_t *)getreversed);
	      //
	      // Get the size of the sent string again, not including the null at the end of the string.
	      //
	      count = valueLen;
	      //
	      // Reset the index we are using.
	      //
	      index = 0;
	      //
	      // Do the shuffle on the number of characters in the string.
	      // BUT ONLY IF THERE ARE CHARACTERS TO SHUFFLE!!!
	      //
	      while( count )reversed[index++] = getreversed[--count];
	      reversed[index++]='\0'; // then properly terminate the string.
	      // Then send the reversed string to the Cloud.
	      ret = af_lib_set_attribute_str( sAf_lib, AF_REVERSED, index ,(const unsigned char *)reversed, AF_LIB_SET_REASON_LOCAL_CHANGE);
	      if (ret != AF_SUCCESS) {
		AFLOG_ERR("my-app: REQUEST:af_lib_set_attribute: failed set for the test attributeId=AF_GETREVERSED");
	      }
	      else {
		AFLOG_INFO("my-app: REQUEST: set attribute id AF_REVERSED succeeded. set to %s",reversed);
	      }

		break;

	    case AF_COUNTBITSOFTHIS:
	      // This attribute will have the number of bits that are set to '1' counted, and then returned in the
	      // attribute named "AF_NUMNBEROFBITS". If there is an error in setting the number of bits attribute,
	      // an error message will be logged. An info message will be logged when it succeeds.
	      // NOTE: It's super important to cast the type of "value" correctly. While it is declared
	      // by the stack to be a uint8_t *, you should treat it as a void *. It will point to whatever
	      // the Cloud has been told that the size of the data item is and it will be of that size.
	      // If you see odd errors, such as values truncating or rolling at 8 or 16 bit intervals, then
	      // it's likely that the casting was not done correctly.
	      AFLOG_INFO( "my-app: SET REQUEST for attrId=AF_COUNTBITSOFTHIS value was=%d",*(uint32_t *)value);
		// Okay, let's use that value.
	      countbitsofthis = *(uint32_t *)value; // keep it in countbitsofthis for a while...
	      // Let the Cloud know we got the attribute.
              af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, 1, (const uint8_t *)&countbitsofthis); 
	      numberofbits = 0; // reset the bit counter.
		//
		// Do the count.
		//
		while( countbitsofthis ) // As long as it's not zero.
		  {
		    if( countbitsofthis & 1 ) numberofbits++;  // If bit one is set, then increment the counter. 
		    countbitsofthis = countbitsofthis >> 1;    // Then rotate-right the thing we are counting bits of.
		  }
		//
		// And send a copy of the result to the Cloud!
		//
	      ret = af_lib_set_attribute_8(sAf_lib, AF_NUMBEROFBITS, numberofbits, AF_LIB_SET_REASON_LOCAL_CHANGE);
	      // Then log the results to the /var/log/messages log.
	      if (ret != AF_SUCCESS) {
                   AFLOG_ERR("my-app: failed set for the test attributeId=AF_NUMBEROFBITS");
               }
	       else {
		 AFLOG_INFO("my-app: set attribute id AF_NUMBEROFBITS succeeded. set to %d",numberofbits);
	       }
	      break;

	    default:
	      //
	      // Here is where all the attributes that are not a control or input to the
	      // device wind up landing. You can create a handler for them if you wish by adding their
	      // attribute ID to the switch statement for the MCU properties; however, in this
	      // demonstration, we are only interested in taking in data items and
	      // processing the ones that need "something done" with them.
	      // The rest of them are results that are sent back and displayed on the mobile app.
	      //
	      AFLOG_INFO( "my-app: MCU_SET_REQUEST EVENT UNHANDLED for attr=%d",attributeId);
                set_succeeded = 0;  // Failed the set.
                af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, valueLen, value);

	    }
            break;


        case AF_LIB_EVENT_MCU_DEFAULT_NOTIFICATION: // Edge attribute changed.
            AFLOG_INFO("my-app: EDGE ATTR changed: for attr=%d", attributeId);
            // Your code to handle whatever needs to be done if you are interesed in a particular attribute.
            break;
	    
	    //
	    // These events are a response to an edge device-generated get request.
	    // Whenever you query for an attribute, this is the event that will hold the response.
	    //
        case AF_LIB_EVENT_ASR_GET_REQUEST: {
            AFLOG_INFO("my-app: EDGE ATTR get_reqeust: for attr=%d", attributeId);
            // Attribute_store asks for the current value of attribute (belonging to edged or MCU). 
            // Responding with attribute and its value.
            // Note 1: af_lib_set_attribute_xx, where xx depends on the type of attributes.

            // Note the following in the call: AF_LIB_SET_REASON_GET_RESPONSE to indicate
            // it is a reply for the get_request.
	    // NOTE: This code is deprecated in this application; however, I'm leaving it here
	    // as an example of how to respond to this particular type of event.
	    //
            //if (attributeId == 1) {
            //    ret = af_lib_set_attribute_8(sAf_lib, attributeId, 
            //                                 attr_id_1_led_button, AF_LIB_SET_REASON_GET_RESPONSE);
            //} 
            //else if (attributeId == 2) {
            //    AFLOG_INFO("my-app: af_lib_set_attriubte_32, with AF_LIB_SET_REASON_GET_RESPONSE");
            //    ret = af_lib_set_attribute_32(sAf_lib, attributeId, 
            //                                 attr_id_2_state, AF_LIB_SET_REASON_GET_RESPONSE);
            //}
            //else {
            //    ret = AF_ERROR_NOT_SUPPORTED;
            //}
	    //
            //if (ret != AF_SUCCESS) {
            //    AFLOG_ERR("my-app: af_lib_set_attribute: failed for request id:%d", attributeId);
            //}
            } // AF_LIB_EVENT_ASR_GET_REQUEST
            break;


        default:
	  AFLOG_INFO("my-app: EVENT: %d, for attribute %d received but not handled", eventType, attributeId);
           break; 

    } // End switch.
}

    //
    // Very simple main loop.. and not actually a loop as it calls event_base_dispatch, which doesn't return
    // until there are no more events to process or until Control C or some other kind of SIGTERM is received.
    //
int main(int argc, char *argv[])
{
  int retVal = AF_SUCCESS;  // Useful return value for when we are done.

   /* Enable pthreads. */
    evthread_use_pthreads();

    /* Get an event_base. */
    sEventBase = event_base_new();
    /* And make sure we actually got one! */
    if (sEventBase == NULL) {
      //
      // Let the world know why we exited.
      //
      AFLOG_ERR("my-app: main_event_base_new::can't allocate event base");
      retVal = -1;
      return (retVal);
    }

    //
    // A little log message so we know the EDGE application code has finally started.
    //
    AFLOG_INFO("my-app: EDGE: start");

    //
    // Register the Afero library's getting us data with the event system.
    //
    retVal = af_lib_set_event_base(sEventBase);
    if (retVal != AF_SUCCESS) {
        AFLOG_ERR("my-app: main_set_event_base::set event base failed");
        goto err_exit;
    }
 
    //
    // Another logging message to keep track of where we are.
    //
    AFLOG_INFO("my-app: EDGE: call af_lib_create_with_unified_callback");
    //
    //   Now create the event and point to our callback function when that event
    //   occurs. Note that this function, af_lib_create_with_unified_callback, will
    //   automatically subscribe you to attributes 1 - 1023, and several Wi-Fi, WAN, and Ethernet 
    //   attributes as well as the Profile change attribute.
    //
    sAf_lib = af_lib_create_with_unified_callback(attrEventCallback, NULL);
    //
    // Make sure the event base was allocated or we will basically be dead in the water.
    //
    if (sAf_lib == NULL) {
      AFLOG_ERR("my-app: main_event_base_new::can't allocate event base"); // And if not, complain about it in the log files.
        retVal = -1;
        goto err_exit;
    }
    AFLOG_INFO("my-app: EDGE: dispatching event base"); 
    //
    //   Start it up! This will not return until
    //   either there are no more events or the loop exit
    //   or kills functions are called.
    //
    event_base_dispatch(sEventBase);
   

err_exit:
    // Done,let's close/clean up. 
    AFLOG_INFO("my-app: EDGED:  shutdown");  // A little log message so we know what's going on.
    af_lib_shutdown();
    return (retVal);
}
