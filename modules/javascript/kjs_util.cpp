
/**
 * Appcelerator Kroll - licensed under the Apache Public License 2
 * see LICENSE in the root folder for details on the license.
 * Copyright (c) 2008 Appcelerator, Inc. All Rights Reserved.
 */
#include "javascript_module.h"

namespace kroll
{
	JSClassRef tibo_class = NULL;
	JSClassRef tibm_class = NULL;
	JSClassRef tibl_class = NULL;
	const JSClassDefinition empty_class = { 0, 0, 0, 0, 0, 0,
	                                        0, 0, 0, 0, 0, 0,
	                                        0, 0, 0, 0, 0 };
	/* callback for BoundObject proxying to KJS */
	void get_property_names_cb(JSContextRef, JSObjectRef, JSPropertyNameAccumulatorRef);
	bool has_property_cb(JSContextRef, JSObjectRef, JSStringRef);
	JSValueRef get_property_cb(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);
	bool set_property_cb(JSContextRef, JSObjectRef, JSStringRef, JSValueRef, JSValueRef*);
	JSValueRef call_as_function_cb(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
	void finalize_cb(JSObjectRef);

	SharedValue KJSUtil::ToKrollValue(JSValueRef value,
	                             JSContextRef ctx,
	                             JSObjectRef this_obj)
	{

		SharedValue kr_val;
		JSValueRef exception = NULL;

		if (value == NULL)
		{
			std::cerr << "Trying to convert NULL JSValueRef!" << std::endl;
			return Value::Undefined;
		}

		if (JSValueIsNumber(ctx, value))
		{
			kr_val = Value::NewDouble(JSValueToNumber(ctx, value, &exception));
		}
		else if (JSValueIsBoolean(ctx, value))
		{
			kr_val = Value::NewBool(JSValueToBoolean(ctx, value));
		}
		else if (JSValueIsString(ctx, value))
		{

			JSStringRef string_ref = JSValueToStringCopy(ctx, value, &exception);
			if (string_ref)
			{
				char* chars = KJSUtil::ToChars(string_ref);
				std::string to_ret = std::string(chars);
				JSStringRelease(string_ref);
				free(chars);
				kr_val = Value::NewString(to_ret);
			}

		}
		else if (JSValueIsObject(ctx, value))
		{

			JSObjectRef o = JSValueToObject(ctx, value, &exception);
			if (o != NULL)
			{
				void* data = (void*) JSObjectGetPrivate(o);
				if (JSObjectIsFunction(ctx, o) && data == NULL)
				{
					// this is a pure JS method: proxy it
					SharedBoundMethod tibm = new KJSBoundMethod(ctx, o, this_obj);
					kr_val = Value::NewMethod(tibm);
				}
				else if (JSObjectIsFunction(ctx, o))
				{
					// this is a BoundMethod: unwrap it
					SharedBoundMethod *tibm = (SharedBoundMethod *) data;
					kr_val = Value::NewMethod(*tibm);
				}
				else if (data == NULL && IsArrayLike(o, ctx))
				{
					// this is a pure JS array: proxy it
					SharedBoundList tibl = new KJSBoundList(ctx, o);
					kr_val = Value::NewList(tibl);
				}
				else if (IsArrayLike(o, ctx))
				{
					// this is a BoundList: unwrap it
					SharedBoundList *tibl = (SharedBoundList*) data;
					kr_val = Value::NewList(*tibl);
				}
				else if (data == NULL)
				{
					// this is a pure JS object: proxy it
					SharedBoundObject tibo = new KJSBoundObject(ctx, o);
					kr_val = Value::NewObject(tibo);
				}
				else
				{
					// this is a kroll::BoundObject: unwrap it
					SharedBoundObject * tibo = (SharedBoundObject*) data;
					kr_val = Value::NewObject(*tibo);
				}
			}

		}
		else if (JSValueIsNull(ctx, value))
		{
			kr_val = kroll::Value::Null;
		}
		else
		{
			kr_val = kroll::Value::Undefined;
		}

		if (!kr_val.isNull() && exception == NULL)
		{
			return kr_val;
		}
		else
		{
			throw KJSUtil::ToKrollValue(exception, ctx, NULL);
		}
	}

	JSValueRef KJSUtil::ToJSValue(SharedValue value, JSContextRef ctx)
	{
		JSValueRef js_val;
		if (value->IsInt())
		{
			js_val = JSValueMakeNumber(ctx, value->ToInt());
		}
		else if (value->IsDouble())
		{
			js_val = JSValueMakeNumber(ctx, value->ToDouble());
		}
		else if (value->IsBool())
		{
			js_val = JSValueMakeBoolean(ctx, value->ToBool());
		}
		else if (value->IsString())
		{
			JSStringRef s = JSStringCreateWithUTF8CString(value->ToString());
			js_val = JSValueMakeString(ctx, s);
			JSStringRelease(s);
		}
		else if (value->IsObject())
		{
			SharedBoundObject obj = value->ToObject();
			SharedPtr<KJSBoundObject> kobj = obj.cast<KJSBoundObject>();
			if (!kobj.isNull() && kobj->SameContextGroup(ctx))
			{
				// this object is actually a pure JS object
				js_val = kobj->GetJSObject();
			}
			else
			{
				// this is a BoundObject that needs to be proxied
				js_val = KJSUtil::ToJSValue(obj, ctx);
			}
		}
		else if (value->IsMethod())
		{
			SharedBoundMethod meth = value->ToMethod();
			SharedPtr<KJSBoundMethod> kmeth = meth.cast<KJSBoundMethod>();
			if (!kmeth.isNull() && kmeth->SameContextGroup(ctx))
			{
				// this object is actually a pure JS callable object
				js_val = kmeth->GetJSObject();
			}
			else
			{
				// this is a TiBoundMethod that needs to be proxied
				js_val = KJSUtil::ToJSValue(meth, ctx);
			}
		}
		else if (value->IsList())
		{
			SharedBoundList list = value->ToList();
			SharedPtr<KJSBoundList> klist = list.cast<KJSBoundList>();
			if (!klist.isNull() && klist->SameContextGroup(ctx))
			{
				// this object is actually a pure JS array
				js_val = klist->GetJSObject();
			}
			else
			{
				// this is a TiBoundMethod that needs to be proxied
				js_val = KJSUtil::ToJSValue(list, ctx);
			}
		}
		else if (value->IsNull())
		{
			js_val = JSValueMakeNull(ctx);
		}
		else if (value->IsUndefined())
		{
			js_val = JSValueMakeUndefined(ctx);
		}
		else
		{
			js_val = JSValueMakeUndefined(ctx);
		}

		return js_val;

	}

	JSValueRef KJSUtil::ToJSValue(SharedBoundObject object, JSContextRef c)
	{
		if (tibo_class == NULL)
		{
			JSClassDefinition js_class_def = empty_class;
			js_class_def.className = strdup("KrollBoundObject");
			js_class_def.getPropertyNames = get_property_names_cb;
			js_class_def.finalize = finalize_cb;
			js_class_def.hasProperty = has_property_cb;
			js_class_def.getProperty = get_property_cb;
			js_class_def.setProperty = set_property_cb;
			tibo_class = JSClassCreate (&js_class_def);
		}
		return JSObjectMake (c, tibo_class, new SharedBoundObject(object));
	}

	JSValueRef KJSUtil::ToJSValue(SharedBoundMethod method, JSContextRef c)
	{
		if (tibm_class == NULL)
		{
			JSClassDefinition js_class_def = empty_class;
			js_class_def.className = strdup("KrollBoundMethod");
			js_class_def.getPropertyNames = get_property_names_cb;
			js_class_def.finalize = finalize_cb;
			js_class_def.hasProperty = has_property_cb;
			js_class_def.getProperty = get_property_cb;
			js_class_def.setProperty = set_property_cb;
			js_class_def.callAsFunction = call_as_function_cb;
			tibm_class = JSClassCreate (&js_class_def);
		}
		return JSObjectMake (c, tibm_class, new SharedBoundMethod(method));
	}

	void inline CopyJSProperty(JSContextRef c,
	                           JSObjectRef from_obj,
	                           SharedBoundObject to_bo,
	                           JSObjectRef to_obj,
	                           const char *prop_name)
	{

		JSStringRef prop_name_str = JSStringCreateWithUTF8CString(prop_name);
		JSValueRef prop = JSObjectGetProperty(c, from_obj, prop_name_str, NULL);
		JSStringRelease(prop_name_str);
		SharedValue prop_val = KJSUtil::ToKrollValue(prop, c, to_obj);
		to_bo->Set(prop_name, prop_val);
	}

	JSValueRef KJSUtil::ToJSValue(SharedBoundList list, JSContextRef c)
	{

		if (tibl_class == NULL)
		{
			JSClassDefinition js_class_def = empty_class;
			js_class_def.className = strdup("KrollBoundList");
			js_class_def.getPropertyNames = get_property_names_cb;
			js_class_def.finalize = finalize_cb;
			js_class_def.hasProperty = has_property_cb;
			js_class_def.getProperty = get_property_cb;
			js_class_def.setProperty = set_property_cb;
			tibl_class = JSClassCreate (&js_class_def);
		}

		JSObjectRef object = JSObjectMake(c, tibl_class, new SharedBoundList(list));

		JSValueRef args[1] = { JSValueMakeNumber(c, 3) };
		JSObjectRef array = JSObjectMakeArray(c, 1, args, NULL);

		/* we are doing this manually because there have been
		   propblems trying to set an object's prototype to array */

		// move some array methods
		CopyJSProperty(c, array, list, object, "push");
		CopyJSProperty(c, array, list, object, "pop");
		CopyJSProperty(c, array, list, object, "shift");
		CopyJSProperty(c, array, list, object, "unshift");
		CopyJSProperty(c, array, list, object, "reverse");
		CopyJSProperty(c, array, list, object, "splice");
		CopyJSProperty(c, array, list, object, "join");
		CopyJSProperty(c, array, list, object, "slice");
		CopyJSProperty(c, array, list, object, "concat");

		return object;
	}

	char* KJSUtil::ToChars(JSStringRef js_string)
	{
		size_t size = JSStringGetMaximumUTF8CStringSize(js_string);
		char* string = (char*) malloc(size);
		JSStringGetUTF8CString(js_string, string, size);
		return string;
	}

	bool KJSUtil::IsArrayLike(JSObjectRef object, JSContextRef c)
	{
		bool array_like = true;

		JSStringRef pop = JSStringCreateWithUTF8CString("pop");
		array_like = array_like && JSObjectHasProperty(c, object, pop);
		JSStringRelease(pop);

		JSStringRef concat = JSStringCreateWithUTF8CString("concat");
		array_like = array_like && JSObjectHasProperty(c, object, concat);
		JSStringRelease(concat);

		JSStringRef length = JSStringCreateWithUTF8CString("length");
		array_like = array_like && JSObjectHasProperty(c, object, length);
		JSStringRelease(length);

		return array_like;
	}

	void finalize_cb(JSObjectRef js_object)
	{
		SharedBoundObject* object = (SharedBoundObject*) JSObjectGetPrivate (js_object);
		delete object;
	}

	void get_property_names_cb(JSContextRef js_context,
	                            JSObjectRef js_object,
	                            JSPropertyNameAccumulatorRef js_properties)
	{
		SharedBoundObject* object = (SharedBoundObject*) JSObjectGetPrivate(js_object);

		if (object == NULL)
			return;

		SharedStringList props = (*object)->GetPropertyNames();
		for (size_t i = 0; i < props->size(); i++)
		{
			JSStringRef name = JSStringCreateWithUTF8CString(props->at(i)->c_str());
			JSPropertyNameAccumulatorAddName(js_properties, name);
			JSStringRelease(name);
		}
	}

	bool has_property_cb(JSContextRef js_context,
	                      JSObjectRef  js_object,
	                      JSStringRef  js_property)
	{
		SharedBoundObject* object = (SharedBoundObject*) JSObjectGetPrivate(js_object);
		if (object == NULL)
			return false;

		char *name = KJSUtil::ToChars(js_property);
		std::string str_name(name);
		free(name);

		SharedStringList names = (*object)->GetPropertyNames();
		for (size_t i = 0; i < names->size(); i++)
		{
			if (str_name == *names->at(i)) {
				return true;
			}
		}

		return false;
	}

	JSValueRef get_property_cb(JSContextRef js_context,
	                            JSObjectRef  js_object,
	                            JSStringRef  js_property,
	                            JSValueRef*  js_exception)
	{
		SharedBoundObject* object = (SharedBoundObject*) JSObjectGetPrivate(js_object);
		if (object == NULL)
			return JSValueMakeUndefined(js_context);

		JSValueRef js_val = NULL;
		char* name = KJSUtil::ToChars(js_property);
		try
		{
			SharedValue ti_val = (*object)->Get(name);
			js_val = KJSUtil::ToJSValue(ti_val, js_context);
		}
		catch (ValueException& exception)
		{
#ifdef DEBUG
			std::cerr << "Exception raised when getting property: " << name << std::endl;
#endif	
			*js_exception = KJSUtil::ToJSValue(exception.GetValue(), js_context);
		}
		catch (std::exception &e)
		{
#ifdef DEBUG
			std::cerr << "Exception raised when getting property: " << name << ", "<< e.what() << std::endl;
#endif	
			SharedValue value = Value::NewString(e.what());
			*js_exception = KJSUtil::ToJSValue(value, js_context);
		}
		catch (...)
		{
			std::cerr << "Caught an exception that I don't know how to handle when getting property: " << name << std::endl;
			SharedValue value = Value::NewString("unknown exception");
			*js_exception = KJSUtil::ToJSValue(value, js_context);
		}

		free(name);
		return js_val;
	}

	bool set_property_cb(JSContextRef js_context,
	                      JSObjectRef  js_object,
	                      JSStringRef  js_property,
	                      JSValueRef   js_value,
	                      JSValueRef*  js_exception)
	{
		SharedBoundObject* object = (SharedBoundObject*) JSObjectGetPrivate(js_object);
		if (object == NULL)
			return false;

		bool propertySet = false;

		char* prop_name = KJSUtil::ToChars(js_property);
		try
		{
			SharedValue ti_val = KJSUtil::ToKrollValue(js_value, js_context, js_object);
			(*object)->Set(prop_name, ti_val);
			propertySet = true;
		}
		catch (ValueException& exception)
		{
#ifdef DEBUG
			std::cerr << "Exception raised when setting property: " << prop_name << std::endl;
#endif	
			*js_exception = KJSUtil::ToJSValue(exception.GetValue(), js_context);
		}
		catch (std::exception &e)
		{
#ifdef DEBUG
			std::cerr << "Exception raised when setting property: " << prop_name << ", "<< e.what() << std::endl;
#endif	
			SharedValue value = Value::NewString(e.what());
			*js_exception = KJSUtil::ToJSValue(value, js_context);
		}
		catch (...)
		{
			SharedValue value = Value::NewString("unknown exception");
			*js_exception = KJSUtil::ToJSValue(value, js_context);
			std::cerr << "Caught an exception that I don't know how to handle setting property: " << prop_name << ", " << std::endl;
		}

		free(prop_name);
		return propertySet;
	}

	JSValueRef call_as_function_cb(JSContextRef     js_context,
	                                JSObjectRef      js_function,
	                                JSObjectRef      js_this,
	                                size_t           num_args,
	                                const JSValueRef js_args[],
	                                JSValueRef*      js_exception)
	{

		SharedBoundObject* method = (SharedBoundObject*) JSObjectGetPrivate(js_function);
		if (method == NULL)
			return JSValueMakeUndefined(js_context);

		ValueList args;
		for (size_t i = 0; i < num_args; i++) {
			SharedValue arg_val = KJSUtil::ToKrollValue(js_args[i], js_context, js_this);
			args.push_back(arg_val);
		}

		JSValueRef js_val = NULL;
		try
		{
			SharedValue ti_val = method->cast<BoundMethod>()->Call(args);
			js_val = KJSUtil::ToJSValue(ti_val, js_context);
		}
		catch (ValueException& exception)
		{
#ifdef DEBUG
 			const char* se = exception.what();
 			std::cerr << "Exception raised when invoking function. Exception: " << se << std::endl;
#endif	
			*js_exception = KJSUtil::ToJSValue(exception.GetValue(), js_context);
		}
		catch (std::exception &e)
		{
#ifdef DEBUG
			std::cerr << "Exception raised when invoking function. Exception: " << e.what() << std::endl;
#endif	
			SharedValue value = Value::NewString(e.what());
			*js_exception = KJSUtil::ToJSValue(value, js_context);
		}
		catch (...)
		{
			SharedValue value = Value::NewString("unknown exception");
			*js_exception = KJSUtil::ToJSValue(value, js_context);
			std::cerr << "Caught an exception that I don't know how to handle when invoking function!" << std::endl;
		}

		return js_val;
	}

	SharedPtr<KJSBoundObject> KJSUtil::ToBoundObject(JSContextRef context,
	                                                 JSObjectRef object)
	{
		return new KJSBoundObject(context, object);
	}

	SharedPtr<KJSBoundMethod> KJSUtil::ToBoundMethod(JSContextRef context,
	                                                 JSObjectRef method,
	                                                 JSObjectRef this_object)
	{
		return new KJSBoundMethod(context, method, this_object);
	}

	SharedPtr<KJSBoundList> KJSUtil::ToBoundList(JSContextRef context,
	                                             JSObjectRef list)
	{
		return new KJSBoundList(context, list);
	}

	std::map<JSObjectRef, JSGlobalContextRef> context_map;
	void KJSUtil::RegisterGlobalContext(
	    JSObjectRef object,
	    JSGlobalContextRef global_ctx)
	{
		context_map[object] = global_ctx;
	}

	JSGlobalContextRef KJSUtil::GetGlobalContext(JSObjectRef object)
	{
		if (context_map.find(object) == context_map.end())
		{
			return NULL;
		}
		else
		{
			return context_map[object];
		}
	}
}
