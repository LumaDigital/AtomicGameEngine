using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace AtomicEngine
{

    internal class RefCountedSafeFileHandle : SafeHandle
    {
        public RefCountedSafeFileHandle(IntPtr handle, bool ownsHandle = true)
            : base(handle, ownsHandle)
        {
            if (handle == IntPtr.Zero)
            {
                throw new InvalidOperationException("RefCountedSafeFileHandle - native == IntPtr.Zero");
            }

            NativeCore.csi_AtomicEngine_AddRef(handle);
        }

        override public bool IsInvalid { get { return handle == IntPtr.Zero; } }

        override protected bool ReleaseHandle()
        {
            if (handle == IntPtr.Zero)
            {
                throw new InvalidOperationException("RefCountedSafeFileHandle.ReleaseHandle - native == IntPtr.Zero");
            }

             NativeCore.csi_AtomicEngine_ReleaseRef(handle);
            
            lock (RefCounted.refCountedFinalizerQueue)
            {
                //RefCounted.refCountedFinalizerQueue.Add(handle);
            }
            

            handle = IntPtr.Zero;

            return true;
        }
    }

    [ComVisible(true)]
    public partial class RefCounted : IDisposable
    {

        // _handle is set to null to indicate disposal of this instance.
        private RefCountedSafeFileHandle refHandle;

        public RefCounted()
        {

        }

        protected RefCounted(IntPtr native)
        {
            nativeInstance = native;
        }

        internal void Init()
        {            
            refHandle = new RefCountedSafeFileHandle(nativeInstance);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);            
        }

        protected virtual void Dispose(bool disposing)
        {

            if (refHandle != null && !refHandle.IsInvalid)
            {
                // Free the handle
                refHandle.Dispose();
            }

            nativeInstance = IntPtr.Zero;

        }

        ~RefCounted()
        {
            // This can run on any thread, so queue release
           // lock (refCountedFinalizerQueue)
           //     refCountedFinalizerQueue.Add(nativeInstance);
        }

        static internal List<IntPtr> refCountedFinalizerQueue = new List<IntPtr>();

        static internal void ReleaseFinalized()
        {
            lock (refCountedFinalizerQueue)
            {
                foreach (var native in refCountedFinalizerQueue)
                {
                    NativeCore.RemoveNative(native);
                    NativeCore.csi_AtomicEngine_ReleaseRef(native);
                }

                refCountedFinalizerQueue.Clear();
            }

        }

        // This method may be called multiple times, called on instance after it is either registered as a new native created in C# (InstantiationType == InstantiationType.INSTANTIATION_NET)
        // or a native which has been wrapped ((InstantiationType != InstantiationType.INSTANTIATION_NET)
        // Note that RefCounted that get GC'd from script, can still live in native code, and get rewrapped 
        internal virtual void PostNativeUpdate()
        {

        }

        public static implicit operator IntPtr(RefCounted refCounted)
        {
            if (refCounted == null)
                return IntPtr.Zero;
                
            return refCounted.nativeInstance;
        }

        public IntPtr NativeInstance { get { return nativeInstance; } }

        public IntPtr nativeInstance = IntPtr.Zero;

        [DllImport(Constants.LIBNAME, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr csi_Atomic_RefCounted_GetClassID(IntPtr self);

    }


}
