package com.nativeflow.root;

public class Root {
	public native static long root();

	static {
		System.loadLibrary("com_nativeflow_root_Root");
	}
}
