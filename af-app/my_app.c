/**
   Copyright 2019 Afero, Inc.
   This program is a demonstration program that shows you how to capture
   an event from the afero cloud to set an edge attribute, and do something with 
   the data that is sent to you. It also inludes the ability to send data
   items from the edge device up to the cloud.  This basic functionality is
   really the basis of all communications with the  afero cloud and the device.
   Receiving an attribute is typicaly used for some kind of control of
   the device, while sending an attribute to the cloud is typically used
   for reporting some kind of operational status from the device back to the
   cloud and mobile app.

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
// and of course, the includes that are afero specific
//
#include "af_attr_def.h"
#include "af_attr_client.h" // AF_ATTR_MAX_LISTEN_RANGES
#include "af_log.h"
#include "af_rpc.h"
#include "af_ipc_server.h"
#include "aflib.h"
#include "aflib_mcu.h"
//
// This include is probably of the most importance to you, the application developer
// since it is created by the Afero Profile Editor and includes all of the definitions
// that map an attribute ID number to the given Name of the attribute that you used
// in the Afero Profile Editor to define those MCU attributes.
// This description file will change every time you edit the attributes in a profile.
// it does not necessarily change if all you change in the Afero Profile Editor is
// the UI layout or the UI elements that are actually used.
// It mostly only denotes a few key items such as data sizes, data types, and attribute name mappings.
//
#include "device-description.h" 

af_lib_t          *sAf_lib    = NULL;  // This is the handle that is used to get and set attributes in the afero library.
struct event_base *sEventBase = NULL;  // Event base for the event handler and callback.



//
// I generally like to group some defines here for debugging purposes. Whenever I run into
// some kind of odd issue that I want to examine closely either through logs or by creating a place
// to move a variable so that I can set a breakpoint in order to examine it, I will create an appropriate
// DEBUG_something define that let's me turn it on and off at compile time.
// You can also use an attribute to control such messaging by simply testing the contents of that
// attribute every time you decide to print a log entry.  Using an attribute of 32 bits in size and
// then assigning bit significances to the bits in that attribute for various parts of the program
// is one such way of doing this.
// Here I have some examples that were used but have been since removed. Mostly leaving this here
// as a tip/technique for debugging.
//
//#define DEBUG_ROTATES 1     // comment out to disble logging successes.
//#define DEBUG_BIT_COUNTS 1

uint16_t  getdoubled     = 0; // value that will get doubled, given to me by the cloud.
uint32_t  doubled        = 0; // value that will get pushed back to the cloud and where the doubling gets deposited.
uint32_t  getrotated     = 0; // value that will get rotated right, given to me by the cloud.
uint32_t  rotate         = 0; // to be rotate value goes here, and then gets sent to the cloud.
uint32_t  rotatedr       = 0; // rotated right value goes here, and then gets sent to the cloud.
uint32_t  rotatedl       = 0; // rotated left value goes here, and then gets sent to the cloud.
uint8_t   getadded       = 0; // added to a running sum value. Comes from the cloud.
uint32_t  currentsum     = 0; // the current summation value that's sent back to the cloud. Running sum
uint8_t   readvarlog     = 0; // used as a bool. set by the cloud. Response is to read var log and send last line.
uint32_t countbitsofthis = 0; // 32bit integer that we will count the bits that are set to one..
uint8_t  numberofbits    = 0; // the result of counting bits in countbitsofthis. This gets sent to cloud.
unsigned char lastlineofvarlog[1536]; // big ole string buffer to respond with.
unsigned char getreversed[1536]; // string sent to us from cloud. must be reversed and sent back..
unsigned char reversed[1536];    // reversed string that is then sent back to the cloud;

//
// I need a file handle as well since one of the things I do is read a line out of /var/log/messages
// as an action in response to an attribute setting.
//
FILE *sync;  // just a file pointer to rummage through /var/log/messages with.


//
// This callback is executed any time ASR has information for the MCU
// The name of this event is defined in the aflib initialization in setup()
// There are several different event types that you can receive.
// in one instance, we have an asr notification event for things like system level changes
// such as the rssi of the wifi changing.
//
// What this demo focuses on at this time, though, is any time an MCU attribute is sent.
// This type of event is known as an AF_LIB_EVENT_MCU_SET_REQUEST event.  It is within
// that event handler that we manage all of our MCU attributes that we have defined with
// the Afero Profile Editor.
//
void attrEventCallback(const af_lib_event_type_t eventType, /* this is the event type */
                       const af_lib_error_t error,          /* This is any error that occured */
                       const uint16_t attributeId,          /* This is the attribute ID number that is being given to us */
                       const uint16_t valueLen,             /* This is the size in bytes of the data being given for that attribute. */
                       const uint8_t* value) /* And this is the actualy value of the attribute. value needs to be cast to the correct thing that it is.*/
  
{
    char hexBuf[80];
    bool set_succeeded = 1;
    uint8_t  ret; 
    int count=0; // I use this to keep track of things when moving character strings around.
    int index=0; // I use this to keep track of things when moving character strings around as well.
    unsigned char *where; // I use this to keep track of the string given to me by the service when the attribute is a string. 

    
    memset(hexBuf, 0, sizeof(hexBuf)); // make sure the buffer is initialized
    if (value != NULL) {
        af_util_convert_data_to_hex_with_name("value", (uint8_t *)value, 
                                          valueLen, hexBuf, sizeof(hexBuf));
    }

    AFLOG_INFO("my-app: attrEventCallback:attrid:%d, %s, len=%d", attributeId, hexBuf, valueLen);
    switch (eventType) {
      //
      // Unsolicited notification when a non-MCU attribute changes state
      // this is the event type that handles such non mcu notifications.
      //
        case AF_LIB_EVENT_ASR_NOTIFICATION: // non-edge attribute notify
            AFLOG_INFO("my-app: NOTIFICATION EVENT: for attr=%d", attributeId);

            //
            // we are interested in attribute wifi rssi (65005)
	    // this is where we would see such an event.
	    // Note that we do nothing with that event except log it to /var/log/messages
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
	    // the service wants to send an MCU attribute value change to the device so that
	    // the device can then "do something" with that attribute.
	    // The most efective way to parse through these is simply a case statement within this
	    // event handler code here.  This is where you would put your code to handle such events.
	    // NOTE: All of these events are handled synchronously here in this example, however
	    // in more complex environments or product functionality, it would be more likely that
	    // it would trigger an assynchronous event that after completion resulted in some thing
	    // (possibly) being sent back to the service with a set attribute function call.
	    // In all cases, however, you must respond to the set event request with an af_lib_send_set_response
	    // indicating that the attribute has been succesfully received.
	    //
        case AF_LIB_EVENT_MCU_SET_REQUEST: // edge attribute set request
            AFLOG_INFO("my-app: MCU_SET_REQUEST EVENT: for attr=%d", attributeId);
            // 
            // as stated above, let's just use a case statement on the attributeId that has been sent.
	    // we will use the lables that the afero profile editor created for us in the form of #defines
	    // in order to make the code more readable. These values are devined in device-description.h
	    //
	    switch( attributeId ){
	      //
	      // this attribute is simply doubled in vaue, and then sent back as attribute AF_DOUBLED
	      //
	    case AF_GETDOUBLED:
	      //
	      // create a log entry so that we know we got here. 
	      AFLOG_INFO( "my-app: SET REQUEST for attrId=AF_GETDOUBLED value was=%d",(uint32_t)*value);
	      //
	      //. go ahead and say we got the data.  We need to give it a variable pointer which
	      // is usually filled with the vaue we got from the service, but that is not necessary.
	      //
	      af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, 1, (const uint8_t *)&getdoubled);
	      //
	      // Here I'm taking the value given by the service which is declared as a uint8_t * and I am
	      // casting it to a uint32_t * since that's what it actually is and then simply mulitplying it by 2.
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
	      getrotated = *(uint32_t *)value; // secure the sent data item.
	      rotatedr = (uint32_t)getrotated >> (uint32_t)1; // rotate the bits right by one.
	      getrotated = *(uint32_t *)value; // secure the sent data item.
	      rotatedl = (uint32_t)getrotated << (uint32_t)1; // aaand to the left.
	      // Then say that we received the value
	      af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, 1,(const uint8_t *) &getrotated);
	      //
	      // doing a name scramble here. Cloud name in ape is ROTATED, 'rotate' is the copy I got from
	      // from the afero stack and then I just copied that value from that attribute into 'rotate'.
	      //
	      //Now I need to create two NEW attributes, but since I know I'll need to I do two things.
	      // mark this section with a TODO: "untie these two directions from rotated and tie
	      // to their own rotatedl and rotatedr attributes. So right now, last one in wins.
	      // NOTE: See if this results in two writes in rapid succession to the cloud or if only the last one
	      // happens.
	      // HOW: to iuntie.. the AF_ROTATED will be AF_ROTATEDR(14) and AF_ROTATEDL(15) and AF_ROTATED will be named AF_ROTATE
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
		currentsum = currentsum + (uint32_t)getadded; // keep it as a running summation.
		//
		//and send a copy to the cloud!
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
		    // Just search for the latest line, do nothing in the loop
		  } 
		  AFLOG_INFO("my-app: REQUEST: Last line %s\n", lastlineofvarlog); //<this is just a log... you can remove it
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
	      count=valueLen; // get the length of the string we are working with.
	      where = value;  // and copy the address of the string being handed to us.
	      AFLOG_INFO( "my-app: SET REQUEST for attrId=%d value was=%s",attributeId,(unsigned char *)value);
	      //
	      // now lets get the string. Just copy the number of characters we were told by
	      // the service that it delivered with "valueLen". Note: This count does NOT include the
	      // terminating NULL for the string.
	      // 
	      while( count-- ) getreversed[index++]=*(unsigned char *)where++;
	      //
	      // then report back to the clouyd that we have gotten the attribute.
	      //
              af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, 1, (const uint8_t *)getreversed);
	      //
	      // Get the size of the sent string again, not including the null at the end of the string.
	      //
	      count = valueLen;
	      //
	      // reset the index we are using.
	      //
	      index = 0;
	      //
	      // Do the shuffle on the number of characters in the string. S
	      // BUT ONLY IF THERE ARE CHARCTERS TO SHUFFLE!!!
	      //
	      while( count )reversed[index++] = getreversed[--count];
	      reversed[index++]='\0'; // then properly terminate the string.
	      // then send the reversed string to the cloud
	      ret = af_lib_set_attribute_str( sAf_lib, AF_REVERSED, index ,(const unsigned char *)reversed, AF_LIB_SET_REASON_LOCAL_CHANGE);
	      if (ret != AF_SUCCESS) {
		AFLOG_ERR("my-app: REQUEST:af_lib_set_attribute: failed set for the test attributeId=AF_GETREVERSED");
	      }
	      else {
		AFLOG_INFO("my-app: REQUEST: set attribute id AF_REVERSED succeeded. set to %s",reversed);
	      }

		break;

	    case AF_COUNTBITSOFTHIS:
	      // This attribute will have the number of bits that are set to one, counted and then returned in the
	      // attribute named "AF_NUMNBEROFBITS"  If there is an error in setting the number of bits attribute,
	      // an error message will be logged. An info message will be logged when it succeeds.
	      // NOTE: It's super important to cast the type of "value" correctly. while it is declared
	      // by the stack to be a uint8_t *, you should treat it as a void *.  It will point to whatever
	      // the cloud has been told that the size of the data item is and it will be of that size.
	      // If you see odd errors such as values truncating or rolling at 8 or 16 bit intervals, then
	      // it's likely that the casting was not done correctly.
	      AFLOG_INFO( "my-app: SET REQUEST for attrId=AF_COUNTBITSOFTHIS value was=%d",*(uint32_t *)value);
		// Okay, let's use that value.
	      countbitsofthis = *(uint32_t *)value; // keep it in countbitsofthis for a while...
	      // let the service know we got the attribute.
              af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, 1, (const uint8_t *)&countbitsofthis); 
	      numberofbits = 0; // reset the bit counter.
		//
		// Do the count
		//
		while( countbitsofthis ) // as long as it's not zero.
		  {
		    if( countbitsofthis & 1 ) numberofbits++;  // if bit one is set then increment the counter. 
		    countbitsofthis = countbitsofthis >> 1;    // then rotate the thing we are counting bits of right
		  }
		//
		//and send a copy of the result to the cloud!
		//
	      ret = af_lib_set_attribute_8(sAf_lib, AF_NUMBEROFBITS, numberofbits, AF_LIB_SET_REASON_LOCAL_CHANGE);
	      // then log the results to the /var/log/messages log.
	      if (ret != AF_SUCCESS) {
                   AFLOG_ERR("my-app: failed set for the test attributeId=AF_NUMBEROFBITS");
               }
	       else {
		 AFLOG_INFO("my-app: set attribute id AF_NUMBEROFBITS succeeded. set to %d",numberofbits);
	       }
	      break;

	    default:
	      //
	      // Here is where all the attributes that are not something that is a control  or input to the
	      // device wind up landing.  you can create a handler for them i you wish by adding their
	      // attribute ID to the switch statement for the MCU properties, however, in this
	      // demonstration, we are only really interested in taking in data items and
	      // processing the ones that need "something done" with them.
	      // the rest of them are results that are sent back and displayed on the mobile application
	      //
	      AFLOG_INFO( "my-app: MCU_SET_REQUEST EVENT UNHANDLED for attr=%d",attributeId);
                set_succeeded = 0;  // failed the set
                af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, valueLen, value);

	    }
            break;


        case AF_LIB_EVENT_MCU_DEFAULT_NOTIFICATION: // edge attribute changed
            AFLOG_INFO("my-app: EDGE ATTR changed: for attr=%d", attributeId);
            // You code to handle whatever needs to be done if you are interesed in a particular attribute
            break;
	    
	    //
	    // These events are events that are a response to an edge device generated get request.
	    // whenever you query for an attribute, this is the event that will hold the response.
	    //
        case AF_LIB_EVENT_ASR_GET_REQUEST: {
            AFLOG_INFO("my-app: EDGE ATTR get_reqeust: for attr=%d", attributeId);
            // attribute_store asks for the current value of attribute 
            // (belong to edged or mcu).  Responding with attribute and its value
            // Note 1: af_lib_set_attribute_xx, where xx depends on the type of attributes 

            // Note the following in the call: AF_LIB_SET_REASON_GET_RESPONSE to indicates
            //  it is a reply for the get_request
	    // NOTE: This code is deprecated in this application, however, I'm leaving it here
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

    } // end switch
}

    //
    // Very simple main loop.. and not actually a loop as it calls event_base_dispatch which doesn't return
    // until there are not more events to process or until Control C or some other kind of SIGTERM is received.
    //
int main(int argc, char *argv[])
{
  int retVal = AF_SUCCESS;  // useful return value for when we are done.

   /* enable pthreads */
    evthread_use_pthreads();

    /* get an event_base */
    sEventBase = event_base_new();
    /* And make sure we actually got one! */
    if (sEventBase == NULL) {
      //
      // let the world know why we exited.
      //
      AFLOG_ERR("my-app: main_event_base_new::can't allocate event base");
      retVal = -1;
      return (retVal);
    }

    //
    // a little log message so that we know the EDGE application code has finally really started.
    //
    AFLOG_INFO("my-app: EDGE: start");

    //
    // register the afero library's getting us data with the event system.
    //
    retVal = af_lib_set_event_base(sEventBase);
    if (retVal != AF_SUCCESS) {
        AFLOG_ERR("my-app: main_set_event_base::set event base failed");
        goto err_exit;
    }
 
    //
    // another logging message to keep track of where we are.
    //
    AFLOG_INFO("my-app: EDGE: call af_lib_create_with_unified_callback");
    //
    //   Now create the event and point to our callback function when that event
    //   occurs. Note that this function af_lib_create_with_unified_callback will
    //   automatically subscribe you to attributes 1 - 1023,  and several wifi, wan and ethernet 
    //   attributes as well as the profile change attribute.
    //
    sAf_lib = af_lib_create_with_unified_callback(attrEventCallback, NULL);
    //
    // Make sure the event base was allocated or we will basically be dead in the water.
    //
    if (sAf_lib == NULL) {
      AFLOG_ERR("my-app: main_event_base_new::can't allocate event base"); // and if not, complain about it in the log files.
        retVal = -1;
        goto err_exit;
    }
    AFLOG_INFO("my-app: EDGE: dispatching event base"); 
    //
    //   start it up! This will not return until
    //   either there are no more events or the loop exit
    //   or kill functions are called.
    //
    event_base_dispatch(sEventBase);
   

err_exit:
    // done,let's close/clean up 
    AFLOG_INFO("my-app: EDGED:  shutdown");  // a little log message so we know what's going on.
    af_lib_shutdown();
    return (retVal);
}
